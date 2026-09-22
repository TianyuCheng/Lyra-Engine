#ifndef LYRA_TESTLIB_HELPER_PIPELING_H
#define LYRA_TESTLIB_HELPER_PIPELING_H

#include "./common.h"

struct SimplePipeline
{
    uint                             vstride       = 0; // vertex buffer stride
    CString                          vshader_entry = "";
    CString                          fshader_entry = "";
    CString                          cshader_entry = "";
    GPUShaderModule                  vshader;
    GPUShaderModule                  fshader;
    GPUShaderModule                  cshader;
    GPUPipelineLayout                playout;
    Vector<GPUBindGroupLayoutHandle> blayouts;
    Vector<GPUColorTargetState>      rstates;
    Optional<GPUDepthStencilState>   dsstate;
    Vector<GPUVertexBufferLayouts>   vlayouts;
    Vector<ShaderAttribute>          attributes;

    void init_color_state(GPUTextureFormat format, bool color_write = true);
    void init_depth_state(GPUTextureFormat format = GPUTextureFormat::DEPTH16UNORM);
    void init_stencil_state(GPUTextureFormat format = GPUTextureFormat::DEPTH24PLUS_STENCIL8);
    void init_depth_stencil_state(GPUTextureFormat format = GPUTextureFormat::DEPTH24PLUS_STENCIL8);
    void init_vshader(GPUDevice& device, ShaderModule* module, CString entry);
    void init_fshader(GPUDevice& device, ShaderModule* module, CString entry);
    void init_cshader(GPUDevice& device, ShaderModule* module, CString entry);
    void init_playout(GPUDevice& device, ShaderReflection* reflection);
};

struct SimpleComputePipeline : public SimplePipeline
{
    GPUComputePipeline pipeline;

    void init_pipeline(GPUDevice& device, ShaderReflection* reflection);
};

struct SimpleRenderPipeline : public SimplePipeline
{
    GPURenderPipeline pipeline;

    void init_pipeline(GPUDevice& device, ShaderReflection* reflection);
};

#endif // LYRA_TESTLIB_HELPER_PIPELING_H
