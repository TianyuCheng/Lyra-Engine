#ifndef LYRA_SAMPLES_EDITOR_RENDERER_H
#define LYRA_SAMPLES_EDITOR_RENDERER_H

#include <Lyra/Lyra.hpp>

using namespace lyra;

struct SampleCubeRenderer
{
public:
    void bind(Application& app);
    void render(const Backbuffer& backbuffer, GPUDevice device, GPUCommandBuffer command);

private:
    void init(Blackboard& blackboard);
    void update(Blackboard& blackboard);
    void destroy(Blackboard& blackboard);
    void init_pipeline(GPUDevice device, Compiler compiler);
    void init_buffers(GPUDevice device);
    void init_bind_group(GPUDevice device);

private:
    GPUBindGroupHeap  heap;
    GPUBindGroup      bind_group;
    GPURenderPipeline pipeline;
    GPUPipelineLayout playout;
    GPUShaderModule   vshader;
    GPUShaderModule   fshader;
    GPUBuffer         vbuffer;
    GPUBuffer         ibuffer;
    GPUBuffer         ubuffer;

    Vector<GPUBindGroupLayoutHandle> blayouts;
};

#endif // LYRA_SAMPLES_EDITOR_RENDERER_H
