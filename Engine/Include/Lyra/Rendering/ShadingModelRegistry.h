#pragma once

#ifndef LYRA_ENGINE_RENDERING_SHADING_MODEL_REGISTRY_H
#define LYRA_ENGINE_RENDERING_SHADING_MODEL_REGISTRY_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/GUID.h>

namespace lyra
{

    enum struct ShadingModel : uint;

    namespace BuiltinShadingModels
    {
        inline constexpr GUID PBR        = 0x1001ull;
        inline constexpr GUID NPR        = 0x1002ull;
        inline constexpr GUID UNLIT      = 0x1003ull;
        inline constexpr GUID SUBSURFACE = 0x1004ull;

        auto from_enum(ShadingModel model) -> GUID;
    } // namespace BuiltinShadingModels

    /**
     * @brief description of a registered shading model archetype.
     */
    struct ShadingModelInfo
    {
        GUID   id;
        String name;
        Path   shader_module_path;
        String entry_point_name = "fsmain";
        String struct_name;
    };

    /**
     * @brief centralized registry treating built-in and user custom shading models equally.
     */
    class ShadingModelRegistry
    {
    public:
        static void register_model(const ShadingModelInfo& info);
        static auto get_model(GUID id) -> Optional<ShadingModelInfo>;
        static auto all_models() -> Vector<ShadingModelInfo>;

        static void init();
        static void shutdown();
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_SHADING_MODEL_REGISTRY_H
