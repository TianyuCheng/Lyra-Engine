#pragma once

#ifndef LYRA_ENGINE_ASSETS_FORMAT_PIPELINEASSET_H
#define LYRA_ENGINE_ASSETS_FORMAT_PIPELINEASSET_H

#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Graphics/RHIDescs.h>

namespace lyra
{
    /**
     * @brief Pipeline types supported by declarative pipeline assets.
     */
    enum struct PipelineType : uint
    {
        RENDER,
        COMPUTE,
    };

    /**
     * @brief Predefined blending modes for pipeline color attachments.
     */
    enum struct BlendPreset : uint
    {
        NONE   = 0,
        OPAQUE = 0,
        ALPHA,
        ADDITIVE,
        PREMULTIPLIED,
    };

    /**
     * @brief Construct standard GPUBlendState from a named blend preset.
     */
    auto get_blend_preset(BlendPreset preset) -> GPUBlendState;

    /**
     * @brief Shader stage definitions for pipeline loading.
     */
    struct PipelineShaderDescriptor
    {
        String module         = "";
        String vertex_entry   = "vsmain";
        String fragment_entry = "fsmain";
        String compute_entry  = "csmain";

        HashMap<String, String> macros = {};
    };

    /**
     * @brief Declarative pipeline asset (.pipeline).
     *
     * Captures render and compute pipeline state configurations in human-readable
     * TOML format directly reusing RHI state descriptors.
     */
    struct PipelineAsset
    {
        static constexpr CString name = "PipelineAsset";

        static constexpr AssetTypeID type = make_uuid("a1b2c3d4-0008-4e56-f789-0123a9c8f008");

        static auto saver() -> AssetSaverAPI;
        static auto loader() -> AssetLoaderAPI;
        static auto cooker() -> AssetCookerAPI;

        PipelineType pipeline_type = PipelineType::RENDER;

        PipelineShaderDescriptor shader = {};

        GPUPrimitiveState primitive = {
            .topology   = GPUPrimitiveTopology::TRIANGLE_LIST,
            .front_face = GPUFrontFace::CCW,
            .cull_mode  = GPUCullMode::BACK,
        };

        GPUMultisampleState multisample = {};

        Optional<GPUDepthStencilState> depth_stencil = std::nullopt;

        Vector<GPUColorTargetState> targets = {};

        /**
         * @brief Serialize to human-readable TOML string.
         */
        auto serialize_toml() const -> String;

        /**
         * @brief Deserialize from TOML string.
         */
        static auto deserialize_toml(StringView toml_text) -> PipelineAsset;

        /**
         * @brief Parse from existing TOML table.
         */
        static auto from_toml(const TOML& table) -> PipelineAsset;

        /**
         * @brief Export to TOML table.
         */
        auto to_toml() const -> TOML;

        /**
         * @brief Constructs GPURenderPipelineDescriptor states based on asset definitions.
         * Target states are stored in target_storage so that the returned view remains valid.
         */
        auto to_render_descriptor(Vector<GPUColorTargetState>& target_storage) const -> GPURenderPipelineDescriptor;

        /**
         * @brief Constructs base GPUComputePipelineDescriptor based on asset definitions.
         */
        auto to_compute_descriptor() const -> GPUComputePipelineDescriptor;
    };

} // namespace lyra

#endif // LYRA_ENGINE_ASSETS_FORMAT_PIPELINEASSET_H
