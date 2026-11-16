#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRASTERIZER_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRASTERIZER_H

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
        int32_t x      = 0;
        int32_t y      = 0;
        int32_t width  = 0;
        int32_t height = 0;
    };

    struct SoftRasterizerContext
    {
        SoftViewport         viewport;
        SoftScissor          scissor;
        GPUPrimitiveState    primitive;
        GPUDepthStencilState depth_stencil;
        GPUColorTargetState  color_target;
        GPUColor             blend_constant{0, 0, 0, 1};
        GPUStencilValue      stencil_reference = 0;

        SoftTexture* color_attachment         = nullptr;
        SoftTexture* depth_stencil_attachment = nullptr;

        // fragment shader callable
        void*                                              fshader_func  = nullptr;
        void*                                              uniform_state = nullptr;
        mutable Function<Vector4(const SoftShadedVertex&)> fragment_shader;
    };

    class SoftRasterizer
    {
    public:
        static auto clip_and_project_triangle(const SoftShadedVertex& v0, const SoftShadedVertex& v1, const SoftShadedVertex& v2,
            const SoftViewport& vp, Vector<SoftRasterTriangle>& out_triangles) -> void;

        static auto rasterize_triangle(const SoftRasterTriangle& tri, const SoftRasterizerContext& ctx) -> void;

        static auto rasterize_triangles(std::span<const SoftRasterTriangle> triangles, const SoftRasterizerContext& ctx) -> void;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTRASTERIZER_H
