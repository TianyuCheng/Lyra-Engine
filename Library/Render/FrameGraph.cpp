// reference: https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
// reference: https://www.gdcvault.com/play/1024045/FrameGraph-Extensible-Rendering-Architecture-in

#include <Lyra/Render/FrameGraph.h>

using namespace lyra;

FrameGraph::~FrameGraph()
{
    for (auto& pass : passes)
        delete pass.entry;

    for (auto& resource : resources)
        // duplicated resources shares the entry with some other resources (avoid double deletion)
        if (!resource.duplicate)
            delete resource.entry;

    passes.clear();
    resources.clear();
}

void FrameGraph::execute(FrameGraphContext* context, FrameGraphAllocator* allocator)
{
    for (auto& pass : passes) {
        // skip culled passes
        if (!pass.active()) continue;

        // create resources
        for (auto& rsid : pass.creates) {
            auto& resource = resources.at(rsid);

            // duplicated resources shares the entry with some other resources (avoid double creation)
            if (!resource.duplicate)
                resource.entry->create(allocator);
            registry.put(rsid, resource.entry);
        }

        // pre-read
        for (auto& read : pass.reads) {
            auto& resource = resources.at(read.resource);
            resource.entry->pre_read(context, pass.entry, read.read_op);
        }

        // pre-write
        for (auto& write : pass.writes) {
            auto& resource = resources.at(write.resource);
            resource.entry->pre_write(context, pass.entry, write.write_op);
        }

        // execute pass callback
        std::invoke(pass.entry->callback, registry, context);

        // delete resources
        for (auto& rsid : pass.deletes) {
            auto& resource = resources.at(rsid);

            // duplicated resources shares the entry with some other resources (avoid double deletion)
            if (!resource.duplicate)
                resource.entry->destroy(allocator);
        }
    }
}

void FrameGraph::compile()
{
    // adding the resources to pass deletes
    for (auto& resource : resources) {
        auto& pass = passes.at(resource.last_pass);
        pass.deletes.push_back(resource.rsid);
    }

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
            if (--pass.refcnt > 0) return;

            // decrement ref counts of resources that it reads if producer.refcnt == 0
            // add them to the stack when their refcnt == 0
            for (auto& res : pass.reads) {
                auto& resource = resources.at(res.resource);
                if (--resource.refcnt == 0)
                    unused_resources.push(resource.rsid);
            }
        });
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
        const auto& resource = resources.at(write.resource);
        for (auto& consumer : resource.consumers) {
            if (visited_passes.find(consumer) == visited_passes.end()) {
                if (has_cycles(visited_passes, recursion_set, consumer))
                    return true;
            } else if (recursion_set.find(consumer) != recursion_set.end()) {
                return true;
            }
        }
        return false;
    }
    recursion_set.erase(psid);
    return false;
}
