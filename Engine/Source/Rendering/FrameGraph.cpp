// reference: https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
// reference: https://www.gdcvault.com/play/1024045/FrameGraph-Extensible-Rendering-Architecture-in

#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

FrameGraph::~FrameGraph() = default;

void FrameGraph::execute(FrameGraphContext* context, FrameGraphAllocator* allocator)
{
    registry.reset(resources.size());

    FrameGraphBarrierBatch barriers;

    for (uint psid : execution_order) {
        auto& pass = passes.at(psid);
        if (!pass.active()) continue;

        // create resources
        for (auto& rsid : pass.creates) {
            auto& resource = resources.at(rsid.id);

            // duplicated resources share the entry with some other resources (avoid double creation)
            if (!resource.duplicate)
                resource.entry->create(allocator);
            registry.put(rsid.id, resource.entry);
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
        barriers.submit(context->cmdlist);

        // execute pass callback
        if (pass.entry && pass.entry->callback) {
            std::invoke(pass.entry->callback, registry, context);
        }

        // delete resources
        for (auto& rsid : pass.deletes) {
            auto& resource = resources.at(rsid.id);

            // duplicated resources share the entry with some other resources (avoid double deletion)
            if (!resource.duplicate)
                resource.entry->destroy(allocator);
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
            if (pass.entry && pass.entry->is_preserved()) return;
            if (--pass.refcnt > 0) return;

            // decrement ref counts of resources that it reads if producer.refcnt == 0
            for (auto& res : pass.reads) {
                auto& res_node = resources.at(res.resource.id);
                if (--res_node.refcnt == 0)
                    unused_resources.push(res_node.rsid);
            }
        });
    }

    // topological sort on active passes using Kahn's algorithm
    Vector<uint> in_degree(passes.size(), 0);
    HashMap<uint, HashSet<uint>> adj;

    for (uint p = 0; p < passes.size(); ++p) {
        auto& pass = passes[p];
        if (!pass.active()) continue;

        // pass reads resources produced by other passes
        for (auto& read : pass.reads) {
            auto& res = resources.at(read.resource.id);
            for (uint prod : res.producers) {
                if (prod != p && passes.at(prod).active()) {
                    if (adj[prod].insert(p).second) {
                        in_degree[p]++;
                    }
                }
            }
        }

        // write-after-read hazards: if a pass writes to a resource already read by previous passes
        for (auto& write : pass.writes) {
            auto& res = resources.at(write.resource.id);
            for (uint consumer : res.consumers) {
                if (consumer != p && consumer < p && passes.at(consumer).active()) {
                    if (adj[consumer].insert(p).second) {
                        in_degree[p]++;
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

    // determine resource lifetimes based on actual execution order
    for (auto& pass : passes) {
        pass.creates.clear();
        pass.deletes.clear();
    }

    HashSet<uint> active_resources;
    HashMap<uint, uint> resource_last_pass;

    for (uint psid : execution_order) {
        auto& pass = passes.at(psid);
        for (auto& read : pass.reads) {
            uint rsid = read.resource.id;
            active_resources.insert(rsid);
            resource_last_pass[rsid] = psid;
        }
        for (auto& write : pass.writes) {
            uint rsid = write.resource.id;
            active_resources.insert(rsid);
            resource_last_pass[rsid] = psid;
        }
    }

    for (uint rsid : active_resources) {
        auto& res = resources.at(rsid);
        if (passes.at(res.creator_pass).active()) {
            passes.at(res.creator_pass).creates.push_back(FrameGraphResource{rsid});
        }
        if (auto it = resource_last_pass.find(rsid); it != resource_last_pass.end()) {
            passes.at(it->second).deletes.push_back(FrameGraphResource{rsid});
        }
    }
}

bool FrameGraph::has_cycles() const
{
    HashSet<uint> visited_passes;
    HashSet<uint> recursion_set;
    for (auto& pass : passes)
        if (visited_passes.find(pass.psid) == visited_passes.end())
            if (has_cycles(visited_passes, recursion_set, pass.psid))
                return true;
    return false;
}

bool FrameGraph::has_cycles(HashSet<uint>& visited_passes, HashSet<uint>& recursion_set, uint psid) const
{
    visited_passes.insert(psid);
    recursion_set.insert(psid);

    const auto& pass = passes.at(psid);
    for (auto& write : pass.writes) {
        const auto& resource = resources.at(write.resource.id);
        for (auto& consumer : resource.consumers) {
            if (visited_passes.find(consumer) == visited_passes.end()) {
                if (has_cycles(visited_passes, recursion_set, consumer))
                    return true;
            } else if (recursion_set.find(consumer) != recursion_set.end()) {
                return true;
            }
        }
    }
    recursion_set.erase(psid);
    return false;
}
