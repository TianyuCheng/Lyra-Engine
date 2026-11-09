#pragma once

#include <filesystem>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/String.h>

using namespace lyra;

namespace lyra::model
{
    namespace fs = std::filesystem;

    inline Logger get_logger()
    {
        static Logger logger = create_logger("Model", LogLevel::trace);
        return logger;
    }

    struct CacheDirs
    {
        Path models;
        Path meshes;
        Path textures;
        Path materials;
    };

    inline CacheDirs ensure_cache_dirs(OSPath caches_root)
    {
        Path      root(caches_root);
        CacheDirs dirs = {
            .models    = root / "models",
            .meshes    = root / "meshes",
            .textures  = root / "textures",
            .materials = root / "materials",
        };
        fs::create_directories(dirs.models);
        fs::create_directories(dirs.meshes);
        fs::create_directories(dirs.textures);
        fs::create_directories(dirs.materials);
        return dirs;
    }
} // namespace lyra::model
