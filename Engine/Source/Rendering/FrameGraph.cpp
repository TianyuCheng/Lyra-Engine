// reference: https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
// reference: https://www.gdcvault.com/play/1024045/FrameGraph-Extensible-Rendering-Architecture-in

#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

FrameGraph::~FrameGraph() = default;

void FrameGraph::execute(FrameGraphContext* context, FrameGraphAllocator* allocator)
{
    assert(allocator && "FrameGraph::execute requires a valid FrameGraphAllocator");

    registry.reset(resources.size());
    for (auto& resource : resources) {
        registry.put(resource.rsid, resource.entry);
    }

    FrameGraphBarrierBatch barriers;

    for (uint psid : execution_order) {
        auto& pass = passes.at(psid);
        if (!pass.active()) continue;

        // create physical resources that become active in this pass
        for (uint pid : pass.creates) {
            physical_resources.at(pid)->create(allocator);
        }

        // pre-read
        for (auto& read : pass.reads) {
            auto& resource = resources.at(read.resource.id);
            resource.entry->pre_read(context, pass.entry.get(), read.read_op, &barriers);
        }

        // pre-write
        for (auto& write : pass.writes) {
            auto& resource = resources.at(write.resource.id);
            resource.entry->pre_write(context, pass.entry.get(), write.write_op, &barriers);
        }

        // flush batched barriers before executing the pass
        barriers.submit(context ? context->cmdlist : GPUCommandBuffer{});

        // debug marker push
        if (context && context->cmdlist.handle.valid() && pass.entry) {
            context->cmdlist.push_debug_group(pass.entry->name.c_str());
        }

        // execute pass callback
        if (pass.entry && pass.entry->callback) {
            std::invoke(pass.entry->callback, registry, context);
        }

        // debug marker pop
        if (context && context->cmdlist.handle.valid() && pass.entry) {
            context->cmdlist.pop_debug_group();
        }

        // delete physical resources whose lifetime ends in this pass
        for (uint pid : pass.deletes) {
            physical_resources.at(pid)->destroy(allocator);
        }
    }
}

void FrameGraph::compile()
{
    // pass.refcnt++ for every resource write
    for (auto& pass : passes)
        pass.refcnt = static_cast<uint>(pass.writes.size());

    // resource.refcnt++ for every resource read
    for (auto& resource : resources)
        resource.refcnt = static_cast<uint>(resource.consumers.size());

    // identify resources with refcnt == 0 and push them on a stack
    Stack<uint> unused_resources;
    for (auto& resource : resources)
        if (resource.refcnt == 0)
            unused_resources.push(resource.rsid);

    // cull unused passes and resources
    while (!unused_resources.empty()) {
        uint rsid = unused_resources.top();
        unused_resources.pop();

        // pop a resource and decrement refcnt of its producer
        auto& resource = resources.at(rsid);
        for_all_producers(resource, [&](auto& pass) {
            if (pass.entry && pass.entry->preserved()) return;
            if (--pass.refcnt > 0) return;

            // decrement ref counts of resources that it reads if producer.refcnt == 0
            for (auto& res : pass.reads) {
                auto& res_node = resources.at(res.resource.id);
                if (--res_node.refcnt == 0)
                    unused_resources.push(res_node.rsid);
            }
        });
    }

    constexpr uint invalid_pass = std::numeric_limits<uint>::max();

    // topological sort on active passes using Kahn's algorithm
    Vector<uint> in_degree(passes.size(), 0);

    HashMap<uint, HashSet<uint>> adj;
    for (uint p = 0; p < passes.size(); ++p) {
        auto& pass = passes[p];
        if (!pass.active()) continue;

        // 1. Read-after-Write dependencies:
        // Pass p reads a resource at a specific version. It depends on the pass that produced that version.
        for (auto& read : pass.reads) {
            auto& res = resources.at(read.resource.id);
            if (auto it = res.version_producers.find(read.resource.version); it != res.version_producers.end()) {
                uint prod = it->second;
                if (prod != invalid_pass && prod != p && passes.at(prod).active()) {
                    if (adj[prod].insert(p).second) {
                        in_degree[p]++;
                    }
                }
            } else if (res.creator_pass != invalid_pass && res.creator_pass != p && passes.at(res.creator_pass).active()) {
                if (adj[res.creator_pass].insert(p).second) {
                    in_degree[p]++;
                }
            }
        }

        // 2. Write dependencies:
        // Pass p writes to a resource at write.resource.version, producing version + 1.
        for (auto& write : pass.writes) {
            auto& res = resources.at(write.resource.id);

            // Previous producer dependency (Write-after-Write)
            if (auto it = res.version_producers.find(write.resource.version); it != res.version_producers.end()) {
                uint prev_prod = it->second;
                if (prev_prod != invalid_pass && prev_prod != p && passes.at(prev_prod).active()) {
                    if (adj[prev_prod].insert(p).second) {
                        in_degree[p]++;
                    }
                }
            }

            // Write-after-Read hazard:
            // Any active pass that read this exact version must complete before pass p overwrites it!
            if (auto it = res.version_consumers.find(write.resource.version); it != res.version_consumers.end()) {
                for (uint consumer : it->second) {
                    if (consumer != p && passes.at(consumer).active()) {
                        if (adj[consumer].insert(p).second) {
                            in_degree[p]++;
                        }
                    }
                }
            }
        }
    }

    Deque<uint> q;
    for (uint p = 0; p < passes.size(); ++p) {
        if (passes[p].active() && in_degree[p] == 0) {
            q.push_back(p);
        }
    }

    execution_order.clear();
    while (!q.empty()) {
        uint u = q.front();
        q.pop_front();
        execution_order.push_back(u);

        if (auto it = adj.find(u); it != adj.end()) {
            for (uint v : it->second) {
                if (--in_degree[v] == 0) {
                    q.push_back(v);
                }
            }
        }
    }

    // verify all active passes were scheduled (DAG cycle detection)
    uint active_count = 0;
    for (auto& pass : passes) {
        if (pass.active()) active_count++;
    }
    assert(execution_order.size() == active_count && "FrameGraph: Cycle detected in render pass dependencies!");

    // determine physical resource lifetimes based on actual execution order
    for (auto& pass : passes) {
        pass.creates.clear();
        pass.deletes.clear();
    }

    struct PhysicalLifetime
    {
        usize first_idx  = ~0ull;
        usize last_idx   = 0;
        uint  first_pass = ~0u;
        uint  last_pass  = ~0u;
        bool  has_access = false;
    };

    Vector<PhysicalLifetime> physical_lifetimes(physical_resources.size());
    for (usize i = 0; i < execution_order.size(); ++i) {
        uint  psid = execution_order[i];
        auto& pass = passes.at(psid);

        auto update_physical = [&](uint rsid) {
            auto& res = resources.at(rsid);
            uint  pid = res.physical_id;
            if (pid < physical_lifetimes.size()) {
                auto& lt      = physical_lifetimes[pid];
                lt.has_access = true;
                if (i < lt.first_idx) {
                    lt.first_idx  = i;
                    lt.first_pass = psid;
                }
                if (i >= lt.last_idx) {
                    lt.last_idx  = i;
                    lt.last_pass = psid;
                }
            }
        };

        for (auto& read : pass.reads)
            update_physical(read.resource.id);
        for (auto& write : pass.writes)
            update_physical(write.resource.id);
    }

    for (usize pid = 0; pid < physical_lifetimes.size(); ++pid) {
        auto& lt = physical_lifetimes[pid];
        if (lt.has_access && physical_resources.at(pid)->type == FrameGraphResourceType::TRANSIENT) {
            passes.at(lt.first_pass).creates.push_back(static_cast<uint>(pid));
            passes.at(lt.last_pass).deletes.push_back(static_cast<uint>(pid));
        }
    }
}

bool FrameGraph::has_cycles() const
{
    uint active_count = 0;
    for (auto& pass : passes) {
        if (pass.active()) active_count++;
    }
    return execution_order.size() != active_count;
}
