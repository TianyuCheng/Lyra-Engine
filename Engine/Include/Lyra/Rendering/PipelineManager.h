#pragma once

#ifndef LYRA_ENGINE_RENDERING_PIPELINEMANAGER_H
#define LYRA_ENGINE_RENDERING_PIPELINEMANAGER_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Result.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/Assets/Format/PipelineAsset.h>

namespace lyra
{
    class PipelineManager;
    class RenderPipelineBuilder;
    class ComputePipelineBuilder;

    /**
     * @brief Synthesized layout reflection data derived directly from Slang metadata.
     */
    struct ReflectedPipelineLayout
    {
        GPUPipelineLayout                    layout;
        Vector<GPUBindGroupLayout>           bind_group_layouts;
        Vector<GPUBindGroupLayoutDescriptor> bind_group_descriptors;
        HashMap<String, uint>                bind_group_locations;
        uint                                 immediate_size = 0;

        auto get_bind_group_layout(uint group) const -> GPUBindGroupLayout
        {
            if (group < bind_group_layouts.size()) {
                return bind_group_layouts[group];
            }
            return GPUBindGroupLayout{};
        }

        auto get_bind_group_location(StringView name) const -> Optional<uint>
        {
            auto it = bind_group_locations.find(String(name));
            if (it != bind_group_locations.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        bool valid() const { return layout.valid(); }
    };

    /**
     * @brief Manages declarative pipeline loading, automated Slang shader reflection,
     * layout synthesis, binary caching, and resource lifecycle.
     */
    class PipelineManager
    {
    public:
        explicit PipelineManager() = default;
        explicit PipelineManager(GPUDevice device, Compiler compiler, FileLoader* loader = nullptr);
        virtual ~PipelineManager();

        PipelineManager(const PipelineManager&)            = delete;
        PipelineManager& operator=(const PipelineManager&) = delete;

        PipelineManager(PipelineManager&& other) noexcept;
        PipelineManager& operator=(PipelineManager&& other) noexcept;

        /**
         * @brief Initialize manager with GPU device, compiler, and optional virtual file loader.
         */
        bool init(GPUDevice device, Compiler compiler, FileLoader* loader = nullptr);

        /**
         * @brief Destroy all cached GPU layouts, shader modules, and pipelines.
         */
        void destroy();

        /**
         * @brief Synthesize GPUPipelineLayout and GPUBindGroupLayouts directly from Slang reflection.
         */
        auto reflect_layout(
            const PipelineShaderDescriptor& shader_desc,
            Span<const ShaderEntryPoint>    extra_entries = {}) -> Result<ReflectedPipelineLayout>;

        /**
         * @brief Derive layout for render pipeline stages (vertex + fragment).
         */
        auto reflect_render_layout(const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>;

        /**
         * @brief Derive layout for compute pipeline stage (compute).
         */
        auto reflect_compute_layout(const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>;

        /**
         * @brief Retrieve or compile a GPUShaderModule from virtual path and entry point.
         */
        auto get_or_compile_shader_module(
            StringView                     module_path,
            StringView                     entry_point,
            const HashMap<String, String>& macros = {}) -> Result<GPUShaderModule>;

        /**
         * @brief Create render pipeline from in-memory PipelineAsset using automated layout derivation.
         */
        auto create_render_pipeline(
            const PipelineAsset&              asset,
            Span<const GPUVertexBufferLayout> vertex_layouts = {},
            CString                           label          = nullptr) -> Result<GPURenderPipeline>;

        /**
         * @brief Create compute pipeline from in-memory PipelineAsset using automated layout derivation.
         */
        auto create_compute_pipeline(
            const PipelineAsset& asset,
            CString              label = nullptr) -> Result<GPUComputePipeline>;

        /**
         * @brief Load declarative .pipeline file from VFS/disk and instantiate GPURenderPipeline.
         */
        auto load_render_pipeline(
            StringView                        virtual_path,
            Span<const GPUVertexBufferLayout> vertex_layouts = {},
            CString                           label          = nullptr) -> Result<GPURenderPipeline>;

        /**
         * @brief Load declarative .pipeline file from VFS/disk and instantiate GPUComputePipeline.
         */
        auto load_compute_pipeline(
            StringView virtual_path,
            CString    label = nullptr) -> Result<GPUComputePipeline>;

        /**
         * @brief Fluent builder for constructing render pipelines declaratively.
         */
        auto render_pipeline_builder() -> RenderPipelineBuilder;

        /**
         * @brief Fluent builder for constructing compute pipelines declaratively.
         */
        auto compute_pipeline_builder() -> ComputePipelineBuilder;

        /**
         * @brief Read a pipeline asset from file without instantiating GPU resources.
         */
        auto load_pipeline_asset(StringView virtual_path) const -> Result<PipelineAsset>;

        /**
         * @brief Accessors.
         */
        auto get_device() const -> GPUDevice { return device; }
        auto get_compiler() const -> Compiler { return compiler; }
        auto get_file_loader() const -> FileLoader* { return loader; }

    private:
        auto compile_module(
            StringView                     module_path,
            const HashMap<String, String>& macros = {}) -> Result<Own<ShaderModule>>;

        auto make_cache_key(
            StringView                     module_path,
            StringView                     entry,
            const HashMap<String, String>& macros) const -> String;

        auto make_layout_cache_key(
            const PipelineShaderDescriptor& shader_desc,
            Span<const ShaderEntryPoint>    extra_entries) const -> String;

        GPUDevice   device;
        Compiler    compiler;
        FileLoader* loader = nullptr;

        // Caches for deduplication
        HashMap<String, ReflectedPipelineLayout> cached_layouts;
        HashMap<String, GPUShaderModule>         cached_shader_modules;
        Vector<GPURenderPipeline>                created_render_pipelines;
        Vector<GPUComputePipeline>               created_compute_pipelines;
        Vector<GPUPipelineLayout>                managed_layouts;
        Vector<GPUBindGroupLayout>               managed_bind_group_layouts;
    };

    /**
     * @brief Fluent builder for constructing GPURenderPipeline instances declaratively.
     */
    class RenderPipelineBuilder
    {
    public:
        explicit RenderPipelineBuilder(PipelineManager& manager);

        auto from_asset(const PipelineAsset& asset) -> RenderPipelineBuilder&;
        auto from_asset(StringView virtual_path) -> RenderPipelineBuilder&;

        auto with_shader(const PipelineShaderDescriptor& shader) -> RenderPipelineBuilder&;
        auto with_shader_module(StringView module_path) -> RenderPipelineBuilder&;
        auto with_vertex_entry(StringView entry) -> RenderPipelineBuilder&;
        auto with_fragment_entry(StringView entry) -> RenderPipelineBuilder&;
        auto with_macro(StringView key, StringView value) -> RenderPipelineBuilder&;

        auto with_vertex_layouts(Span<const GPUVertexBufferLayout> layouts) -> RenderPipelineBuilder&;
        auto with_vertex_layout(const GPUVertexBufferLayout& layout) -> RenderPipelineBuilder&;

        auto with_primitive(const GPUPrimitiveState& primitive) -> RenderPipelineBuilder&;
        auto with_topology(GPUPrimitiveTopology topology) -> RenderPipelineBuilder&;
        auto with_cull_mode(GPUCullMode cull_mode) -> RenderPipelineBuilder&;
        auto with_front_face(GPUFrontFace front_face) -> RenderPipelineBuilder&;

        auto with_depth_stencil(const GPUDepthStencilState& ds) -> RenderPipelineBuilder&;
        auto with_depth_stencil(
            GPUTextureFormat   format        = GPUTextureFormat::DEPTH32FLOAT,
            bool               write_enabled = true,
            GPUCompareFunction compare       = GPUCompareFunction::GREATER_EQUAL) -> RenderPipelineBuilder&;
        auto without_depth_stencil() -> RenderPipelineBuilder&;

        auto with_multisample(const GPUMultisampleState& ms) -> RenderPipelineBuilder&;

        auto with_targets(Span<const GPUColorTargetState> targets) -> RenderPipelineBuilder&;
        auto with_target(const GPUColorTargetState& target) -> RenderPipelineBuilder&;
        auto with_target(
            GPUTextureFormat   format,
            BlendPreset        blend      = BlendPreset::NONE,
            GPUColorWriteFlags write_mask = GPUColorWrite::ALL) -> RenderPipelineBuilder&;

        auto with_label(CString label) -> RenderPipelineBuilder&;

        auto build() -> Result<GPURenderPipeline>;

    private:
        PipelineManager&              manager;
        PipelineAsset                 asset;
        Vector<GPUVertexBufferLayout> vertex_layouts;
        CString                       label          = nullptr;
        bool                          custom_targets = false;
        Optional<String>              error_message;
    };

    /**
     * @brief Fluent builder for constructing GPUComputePipeline instances declaratively.
     */
    class ComputePipelineBuilder
    {
    public:
        explicit ComputePipelineBuilder(PipelineManager& manager);

        auto from_asset(const PipelineAsset& asset) -> ComputePipelineBuilder&;
        auto from_asset(StringView virtual_path) -> ComputePipelineBuilder&;

        auto with_shader(const PipelineShaderDescriptor& shader) -> ComputePipelineBuilder&;
        auto with_shader_module(StringView module_path) -> ComputePipelineBuilder&;
        auto with_compute_entry(StringView entry) -> ComputePipelineBuilder&;
        auto with_macro(StringView key, StringView value) -> ComputePipelineBuilder&;

        auto with_label(CString label) -> ComputePipelineBuilder&;

        auto build() -> Result<GPUComputePipeline>;

    private:
        PipelineManager& manager;
        PipelineAsset    asset;
        CString          label = nullptr;
        Optional<String> error_message;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_PIPELINEMANAGER_H
