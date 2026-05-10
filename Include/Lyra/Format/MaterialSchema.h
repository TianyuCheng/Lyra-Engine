#ifndef LYRA_LYRA_FORMAT_MATERIALSCHEMA_H
#define LYRA_LYRA_FORMAT_MATERIALSCHEMA_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Render/RHIEnums.h>
#include <Lyra/Format/TextureAsset.h>
#include <Lyra/Effect/MaterialGraph.h>

// avoid collision with Windows GDI
#undef OPAQUE

namespace lyra
{

    /**
     * @brief Common semantics for material parameters, mainly for PBR.
     */
    enum struct MaterialSemantic : uint
    {
        NONE,
        ALBEDO,
        NORMAL,
        METALLIC,
        ROUGHNESS,
        OCCLUSION,
        EMISSIVE,
        SPECULAR,
        OPACITY,
        DISPLACEMENT,
        CUSTOM0,
        CUSTOM1,
        CUSTOM2,
        CUSTOM3,
        CUSTOM4,
        CUSTOM5,
        CUSTOM6,
        CUSTOM7,
    };

    /**
     * @brief Supported parameter types for materials.
     */
    enum struct MaterialParameterType : uint
    {
        UINT,
        UINT2,
        UINT3,
        UINT4,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        TEXTURE2D,
        TEXTURE_CUBE
    };

    /**
     * @brief A single parameter definition in a material schema.
     */
    struct MaterialParameter
    {
        String                name;
        MaterialParameterType type;
        MaterialSemantic      semantic      = MaterialSemantic::NONE;
        Vector4               default_value = Vector4(0.0f);
    };

    /**
     * @brief Blending modes for materials.
     */
    enum struct MaterialBlendMode : uint
    {
        OPAQUE,
        MASK,
        BLEND
    };

    /**
     * @brief A schema defining the properties and algorithm for a class of materials.
     */
    struct MaterialSchema
    {
        static constexpr CString name = "MaterialSchema";

        static constexpr AssetTypeID type = 0x8a1f2b3c;

        static auto loader() -> AssetLoaderAPI;

        uint                      schema_id = 0;
        String                    display_name;
        Vector<MaterialParameter> parameters;
        MaterialGraph             graph;

        // default pipeline states
        MaterialBlendMode blend_mode  = MaterialBlendMode::OPAQUE;
        GPUCullMode       cull_mode   = GPUCullMode::BACK;
        bool              depth_write = true;
        bool              depth_test  = true;
    };

    /**
     * @brief Container for actual resource values used in a material.
     */
    struct MaterialParams
    {
        HashMap<String, TextureAssetHandle> textures;
        HashMap<String, Vector4>            constants;
    };

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_MATERIALSCHEMA_H
