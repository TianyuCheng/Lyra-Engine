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

        void destroy()
        {
            if (layout.valid()) {
                layout.destroy();
                layout = {};
            }
            for (auto& bgl : bind_group_layouts) {
                if (bgl.valid()) {
                    bgl.destroy();
                }
            }
            bind_group_layouts.clear();
            bind_group_descriptors.clear();
            bind_group_locations.clear();
            immediate_size = 0;
        }

        void destroy(GPUDevice) { destroy(); }
    };

    /**
     * @brief Complete GPU render pipeline package containing the pipeline and its reflected layout hierarchy.
     */
    struct PipelineBundle
    {
        GPURenderPipeline       pipeline;
        ReflectedPipelineLayout layout;
        Vector<GPUShaderModule> shader_modules;

        bool valid() const { return pipeline.valid(); }
        operator GPURenderPipeline() const { return pipeline; }

        void destroy()
        {
            if (pipeline.valid()) {
                pipeline.destroy();
                pipeline = {};
            }
            for (auto& sm : shader_modules) {
                if (sm.valid()) {
                    sm.destroy();
                }
            }
            shader_modules.clear();
            layout.destroy();
        }

        void destroy(GPUDevice) { destroy(); }
    };

    /**
     * @brief Complete GPU compute pipeline package containing the compute pipeline and its reflected layout hierarchy.
     */
    struct ComputePipelineBundle
    {
        GPUComputePipeline      pipeline;
        ReflectedPipelineLayout layout;
        Vector<GPUShaderModule> shader_modules;

        bool valid() const { return pipeline.valid(); }
        operator GPUComputePipeline() const { return pipeline; }

        void destroy()
        {
            if (pipeline.valid()) {
                pipeline.destroy();
                pipeline = {};
            }
            for (auto& sm : shader_modules) {
                if (sm.valid()) {
                    sm.destroy();
                }
            }
            shader_modules.clear();
            layout.destroy();
        }

        void destroy(GPUDevice) { destroy(); }
    };

    /**
     * @brief Stateless reflection and shader synthesis helpers.
     */
    namespace PipelineReflection
    {
        auto reflect_layout(
            GPUDevice                       device,
            Compiler                        compiler,
            FileLoader*                     loader,
            const PipelineShaderDescriptor& shader_desc,
            Span<const ShaderEntryPoint>    extra_entries = {}) -> Result<ReflectedPipelineLayout>;

        auto reflect_render_layout(
            GPUDevice                       device,
            Compiler                        compiler,
            FileLoader*                     loader,
            const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>;

        auto reflect_compute_layout(
            GPUDevice                       device,
            Compiler                        compiler,
            FileLoader*                     loader,
            const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>;
    }

    /**
     * @brief Fluent standalone builder for constructing GPURenderPipeline instances declaratively.
     */
    class RenderPipelineBuilder
    {
    public:
        explicit RenderPipelineBuilder(GPUDevice device, Compiler compiler, FileLoader* loader = nullptr);
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

        /**
         * @brief Builds pipeline and reflected layout into a self-contained PipelineBundle.
         * Logs errors directly via spdlog on failure and returns an invalid bundle.
         */
        auto build() -> PipelineBundle;

    private:
        GPUDevice                     device;
        Compiler                      compiler;
        FileLoader*                   loader         = nullptr;
        PipelineAsset                 asset;
        Vector<GPUVertexBufferLayout> vertex_layouts;
        CString                       label          = nullptr;
        bool                          custom_targets = false;
        Optional<String>              error_message;
    };

    /**
     * @brief Fluent standalone builder for constructing GPUComputePipeline instances declaratively.
     */
    class ComputePipelineBuilder
    {
    public:
        explicit ComputePipelineBuilder(GPUDevice device, Compiler compiler, FileLoader* loader = nullptr);
        explicit ComputePipelineBuilder(PipelineManager& manager);

        auto from_asset(const PipelineAsset& asset) -> ComputePipelineBuilder&;
        auto from_asset(StringView virtual_path) -> ComputePipelineBuilder&;

        auto with_shader(const PipelineShaderDescriptor& shader) -> ComputePipelineBuilder&;
        auto with_shader_module(StringView module_path) -> ComputePipelineBuilder&;
        auto with_compute_entry(StringView entry) -> ComputePipelineBuilder&;
        auto with_macro(StringView key, StringView value) -> ComputePipelineBuilder&;

        auto with_label(CString label) -> ComputePipelineBuilder&;

        /**
         * @brief Builds compute pipeline and reflected layout into a ComputePipelineBundle.
         * Logs errors directly via spdlog on failure and returns an invalid bundle.
         */
        auto build() -> ComputePipelineBundle;

    private:
        GPUDevice        device;
        Compiler         compiler;
        FileLoader*      loader = nullptr;
        PipelineAsset    asset;
        CString          label  = nullptr;
        Optional<String> error_message;
    };

    /**
     * @brief Utility coordinator for shader reflection, pipeline builders, and asset loading.
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
         * @brief Clear cached reflection layouts and shader modules.
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
         * @brief Create render pipeline bundle from in-memory PipelineAsset using automated layout derivation.
         */
        auto create_render_pipeline(
            const PipelineAsset&              asset,
            Span<const GPUVertexBufferLayout> vertex_layouts = {},
            CString                           label          = nullptr) -> PipelineBundle;

        /**
         * @brief Create compute pipeline bundle from in-memory PipelineAsset using automated layout derivation.
         */
        auto create_compute_pipeline(
            const PipelineAsset& asset,
            CString              label = nullptr) -> ComputePipelineBundle;

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
        GPUDevice   device;
        Compiler    compiler;
        FileLoader* loader = nullptr;

        HashMap<String, ReflectedPipelineLayout> cached_layouts;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_PIPELINEMANAGER_H
