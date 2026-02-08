#pragma once

#ifndef LYRA_LIBRARY_SHADER_SLC_TYPES_H
#define LYRA_LIBRARY_SHADER_SLC_TYPES_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Shader/SLCEnums.h>
#include <Lyra/Shader/SLCUtils.h>
#include <Lyra/Shader/SLCDescs.h>
#include <Lyra/Render/RHIEnums.h>
#include <Lyra/Render/RHIDescs.h>

namespace lyra
{
    struct ShaderAPI;

    struct ShaderModule
    {
        ShaderModuleHandle handle;

        virtual ~ShaderModule();

        operator ShaderModuleHandle() { return handle; }
        operator ShaderModuleHandle() const { return handle; }

        auto get_shader_blob(CString entry) const -> OwnedShaderBlob;
    };

    struct ShaderReflection
    {
        ShaderReflectionHandle handle;

        virtual ~ShaderReflection();

        operator ShaderReflectionHandle() { return handle; }
        operator ShaderReflectionHandle() const { return handle; }

        auto get_bind_group_location(CString name) const -> uint;
        auto get_bind_group_layouts() -> GPUBindGroupLayoutDescriptors;
        auto get_push_constant_ranges() -> GPUPushConstantRanges;
        auto get_vertex_attributes(ShaderAttributes attrs) -> GPUVertexAttributes;
        auto get_vertex_attributes(InitList<ShaderAttribute> attrs) -> GPUVertexAttributes;

    private:
        List<Vector<GPUVertexAttribute>>     vertex_attributes;
        Vector<GPUBindGroupLayoutDescriptor> bind_group_layouts;
        Vector<GPUPushConstantRange>         push_constant_ranges;
    };

    struct Compiler
    {
        CompilerHandle handle;

        static auto init(const CompilerDescriptor& descriptor) -> OwnedResource<Compiler>;

        static auto api() -> ShaderAPI*;

        // implicit conversion
        Compiler() : handle() {}
        Compiler(CompilerHandle handle) : handle(handle) {}

        operator CompilerHandle() { return handle; }
        operator CompilerHandle() const { return handle; }

        void destroy();

        auto compile(const Path& path) -> Own<ShaderModule>;

        auto compile(const CompileDescriptor& descriptor) -> Own<ShaderModule>;

        auto reflect(InitList<ShaderEntryPoint> entry_points) -> Own<ShaderReflection>;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_SHADER_SLC_TYPES_H
