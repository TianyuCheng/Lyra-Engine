#pragma once

#ifndef LYRA_ENGINE_FILESYSTEM_VFSPATH_H
#define LYRA_ENGINE_FILESYSTEM_VFSPATH_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Macros.h>

namespace lyra
{
    namespace vfs
    {
        inline constexpr StringView SchemeSys = "sys";
        inline constexpr StringView SchemeApp = "app";
    } // namespace vfs

    /**
     * @brief Represents a parsed virtual file system path with a protocol scheme.
     *        Examples: "sys://shaders/pbr.hlsl", "app://textures/wood.png".
     */
    struct VFSPath
    {
        String scheme; ///< Protocol scheme (e.g. "sys", "app")
        String path;   ///< Normalized subpath without leading or trailing slashes

        VFSPath() = default;
        VFSPath(StringView scheme, StringView path);

        /**
         * @brief Parse a raw virtual path string.
         *        A protocol scheme ("<scheme>://") is strictly required.
         *        If missing, returns an invalid VFSPath (is_valid() == false).
         */
        static auto parse(StringView raw_vpath) -> VFSPath;

        /**
         * @brief Check whether a path contains a protocol prefix ("<scheme>://").
         */
        static bool has_protocol(StringView raw_vpath);

        /**
         * @brief Format to canonical string: "<scheme>://<path>" (or "<scheme>://" if path is empty).
         */
        auto to_string() const -> String;

        FORCE_INLINE bool is_sys() const { return scheme == vfs::SchemeSys; }
        FORCE_INLINE bool is_app() const { return scheme == vfs::SchemeApp; }
        FORCE_INLINE bool is_empty() const { return scheme.empty() && path.empty(); }
        FORCE_INLINE bool is_valid() const { return !scheme.empty(); }

        bool operator==(const VFSPath& other) const = default;
    };

} // namespace lyra

#endif // LYRA_ENGINE_FILESYSTEM_VFSPATH_H
