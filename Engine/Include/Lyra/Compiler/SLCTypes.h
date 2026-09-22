#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCTYPES_H
#define LYRA_ENGINE_COMPILER_SLCTYPES_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Compiler/SLCUtils.h>
#include <Lyra/Compiler/SLCDescs.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIDescs.h>

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

#endif // LYRA_ENGINE_COMPILER_SLCTYPES_H
