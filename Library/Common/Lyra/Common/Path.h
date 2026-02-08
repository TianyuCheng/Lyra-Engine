#pragma once

#ifndef LYRA_LIBRARY_COMMON_PATH_H
#define LYRA_LIBRARY_COMMON_PATH_H

#include <filesystem>

#include <Lyra/Common/String.h>

namespace lyra
{
    namespace fs = std::filesystem;

    // C++ path type
    using Path = std::filesystem::path;

    // use OS preferred path type for real file system path
    using OSPath = const std::filesystem::path::value_type*;

    // use const char* for (possibly) virtual file system path
    using FSPath = CString;

    // find git root directory from current directory
    inline Path git_root()
    {
        // start from the current working directory
        Path current_path = std::filesystem::current_path();

        // iterate upwards until we find a .git directory or hit the root
        while (current_path.has_parent_path()) {
            // check if current path contains .git directory
            if (std::filesystem::exists(current_path / ".git"))
                return current_path;

            // we have reached the root of the filesystem and found nothing.
            if (current_path == current_path.parent_path())
                break;

            current_path = current_path.parent_path();
        }

        // final check on the last path if we broke out of the loop
        if (std::filesystem::exists(current_path / ".git"))
            return current_path;

        // Return an empty path if not found
        throw std::runtime_error("Not under a git repository!");
        return Path();
    }
} // end of namespace lyra

#endif // LYRA_LIBRARY_COMMON_PATH_H
