#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRASTERIZER_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRASTERIZER_H

#include "SoftCommon.h"
#include "SoftTexture.h"

namespace lyra::softgpu
{
    struct SoftShadedVertex
    {
        Vector4 position{0.0f}; // clip space (X, Y, Z, W)
        Vector4 color{1.0f};
        Vector4 texcoord{0.0f};
        Vector4 normal{0.0f};
        Vector4 extra{0.0f};
        float   varyings[16]{};
        float   inv_w = 1.0f;
        Vector3 screen{0.0f}; // screen space (x, y, z)
    };

    struct SoftRasterTriangle
    {
        SoftShadedVertex v0;
        SoftShadedVertex v1;
        SoftShadedVertex v2;
    };

    struct SoftViewport
    {
        float x         = 0.0f;
        float y         = 0.0f;
        float width     = 0.0f;
        float height    = 0.0f;
        float min_depth = 0.0f;
        float max_depth = 1.0f;
    };

    struct SoftScissor
    {
        int x      = 0;
        int y      = 0;
        int width  = 0;
        int height = 0;
    };

    struct SoftRasterizerContext
    {
        SoftViewport                viewport;
        SoftScissor                 scissor;
        GPUPrimitiveState           primitive;
        GPUDepthStencilState        depth_stencil;
        GPUColorTargetState         color_target;
        Vector<GPUColorTargetState> color_targets;
        GPUColor                    blend_constant{0, 0, 0, 1};
        GPUStencilValue             stencil_reference = 0;

        SoftTexture*         color_attachment = nullptr;
        Vector<SoftTexture*> color_attachments;
        SoftTexture*         depth_stencil_attachment = nullptr;

        // fragment shader callable
        void*                                                                                  fshader_func  = nullptr;
        ushort                                                                                 depth_offset  = 0xFFFF;
        void*                                                                                  uniform_state = nullptr;
        mutable Function<Vector4(const SoftShadedVertex&)>                                     fragment_shader;
        mutable Function<void(const SoftShadedVertex&, Vector4* out_colors, uint max_targets)> fragment_shader_mrt;
    };

    class SoftRasterizer
    {
    public:
        static void clip_and_project_triangle(const SoftShadedVertex& v0, const SoftShadedVertex& v1, const SoftShadedVertex& v2,
            const SoftViewport& vp, Vector<SoftRasterTriangle>& out_triangles);

        static void rasterize_triangle(const SoftRasterTriangle& tri, const SoftRasterizerContext& ctx);

        static void rasterize_triangles(std::span<const SoftRasterTriangle> triangles, const SoftRasterizerContext& ctx);
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTRASTERIZER_H
