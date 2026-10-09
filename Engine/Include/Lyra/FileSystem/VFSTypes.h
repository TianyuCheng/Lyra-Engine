#pragma once

#ifndef LYRA_ENGINE_FILESYSTEM_VFSTYPES_H
#define LYRA_ENGINE_FILESYSTEM_VFSTYPES_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSAPI.h>

namespace lyra
{
    struct FileLoader
    {
    public:
        explicit FileLoader(FSLoader loader);
        explicit FileLoader(FileLoaderAPI* api, FileLoaderHandle loader);

        void destroy();

        bool exists(FSPath vpath) const;

        auto size(FSPath vpath) const -> size_t;

        auto open(FSPath vpath) const -> FileHandle;

        void close(FileHandle file) const;

        template <typename T>
        auto read(FSPath vpath) const -> Vector<T>
        {
            auto sz = size(vpath);
            if (sz == 0) return {};
            Vector<T> data(sz / sizeof(T), 0);
            if (!api_->read_whole_file(loader, vpath, reinterpret_cast<void*>(data.data()))) {
                return {};
            }
            return data;
        }

        auto read(FileHandle file, void* buffer, size_t size) const -> size_t;

        void seek(FileHandle file, int64_t offset) const;

        auto mount(FSPath vpath, const Path& path, uint priority) const -> MountHandle;

        auto mount(FSPath vpath, OSPath path, uint priority) const -> MountHandle;

        void unmount(MountHandle mount) const;

        FORCE_INLINE FileLoaderAPI* api() { return api_; }

        FORCE_INLINE FileLoaderAPI* api() const { return api_; }

        FORCE_INLINE FileLoaderHandle handle() { return loader; }

        FORCE_INLINE FileLoaderHandle handle() const { return loader; }

    private:
        FileLoaderAPI*   api_ = nullptr;
        FileLoaderHandle loader;
    };

    struct FilePacker
    {
    public:
        explicit FilePacker(FSPacker packer, OSPath path);
        explicit FilePacker(FilePackerAPI* api, FilePackerHandle packer);

        void destroy();

        void write(FSPath vpath, void* buffer, size_t size) const;

        void write(FSPath vpath, const Path& path) const;

        FORCE_INLINE FilePackerAPI* api() { return api_; }

        FORCE_INLINE FilePackerAPI* api() const { return api_; }

        FORCE_INLINE FilePackerHandle handle() { return packer; }

        FORCE_INLINE FilePackerHandle handle() const { return packer; }

    private:
        FilePackerAPI*   api_ = nullptr;
        FilePackerHandle packer;
    };

} // namespace lyra

#endif // LYRA_ENGINE_FILESYSTEM_VFSTYPES_H
