#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

FrameGraphBuilder::FrameGraphBuilder()
{
    graph = std::make_unique<FrameGraph>();
}

FrameGraphPass& FrameGraphBuilder::create_pass(StringView name)
{
    auto psid = static_cast<uint>(graph->passes.size());
    auto pass = std::make_unique<FrameGraphPass>(name);
    auto node = FrameGraphPassNode{std::move(pass), psid};
    graph->passes.push_back(std::move(node));
    this->pass = psid;
    return *graph->passes.back().entry;
}

Own<FrameGraph> FrameGraphBuilder::build()
{
    graph->compile();
    return std::move(graph);
}
