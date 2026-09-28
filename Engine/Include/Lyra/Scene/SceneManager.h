#pragma once

#ifndef LYRA_ENGINE_SCENE_SCENE_MANAGER_H
#define LYRA_ENGINE_SCENE_SCENE_MANAGER_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/Mesh.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>

namespace lyra
{
    /**
     * @brief scene loading modes.
     */
    enum struct LoadMode : uint
    {
        SINGLE,  ///< clears current world entities (except DontDestroyOnLoad) and sets new scene as active.
        ADDITIVE ///< loads and spawns scene alongside existing entities without clearing.
    };

    /**
     * @brief parameters for spawning scene or model hierarchies.
     */
    struct SpawnParams
    {
        SceneNode  parent   = {};                          ///< optional parent entity.
        Vector3    position = Vector3(0.0f);               ///< local offset applied to root.
        Quaternion rotation = glm::identity<Quaternion>(); ///< local rotation applied to root.
        Vector3    scale    = Vector3(1.0f);               ///< local scale applied to root.
    };

    /**
     * @brief runtime representation of a loaded scene instance.
     */
    struct SceneInstance
    {
        SceneInstanceID  id     = INVALID_SCENE_INSTANCE;
        SceneAssetHandle handle = {};
        String           name   = "";
        SceneNode        root;
        bool             is_active = false;
    };

    /**
     * @brief manages scene lifecycle, spawning, and hierarchy serialization.
     */
    class SceneManager
    {
    public:
        using LoadedCallback   = Function<void(SceneInstanceID)>;
        using UnloadedCallback = Function<void(SceneInstanceID)>;

        explicit SceneManager(World& world, SceneTree& hierarchy, AssetServer& ams);
        virtual ~SceneManager() = default;

        // --- object / prefab spawning ---

        /**
         * @brief spawn a ModelAsset prefab into the world (returns root SceneNode).
         */
        auto spawn(
            ModelAssetHandle   model,
            const SpawnParams& params = {}) -> SceneNode;

        // --- scene / level lifecycle ---

        /**
         * @brief load and activate a scene (SINGLE or ADDITIVE).
         */
        auto load(
            SceneAssetHandle handle,
            LoadMode         mode = LoadMode::SINGLE) -> SceneInstanceID;

        auto load(
            SceneAssetHandle   handle,
            const SpawnParams& params,
            LoadMode           mode = LoadMode::SINGLE) -> SceneInstanceID;

        bool unload(SceneInstanceID instance_id);
        void clear();

        // --- serialization ---

        auto serialize() const -> SceneAsset;

        // --- queries & updates ---

        auto get_active() const -> const SceneInstance*;
        auto get_active_name() const -> String;
        auto get_instance(SceneInstanceID id) const -> const SceneInstance*;

        void on_loaded(LoadedCallback cb) { loaded_callbacks.push_back(std::move(cb)); }
        void on_unloaded(UnloadedCallback cb) { unloaded_callbacks.push_back(std::move(cb)); }

        void update();

    private:
        SceneNode spawn_model_internal(
            const ModelAsset&  model,
            const SpawnParams& params);

        SceneNode spawn_scene_internal(
            const SceneAsset&  scene,
            const SpawnParams& params);

    private:
        World&                                  world;
        SceneTree&                              hierarchy;
        AssetServer&                            ams;
        SceneInstanceID                         next_instance_id   = 1;
        SceneInstanceID                         active_instance_id = INVALID_SCENE_INSTANCE;
        HashMap<SceneInstanceID, SceneInstance> instances;
        Vector<LoadedCallback>                  loaded_callbacks;
        Vector<UnloadedCallback>                unloaded_callbacks;
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCENE_SCENE_MANAGER_H
