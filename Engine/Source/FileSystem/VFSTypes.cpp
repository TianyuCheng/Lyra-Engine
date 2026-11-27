#include <fstream>

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Assert.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/FileSystem/VFSTypes.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::loader::native, FileLoaderAPI)
FORWARD_DECLARE_API(lyra::loader::physfs, FileLoaderAPI)
FORWARD_DECLARE_API(lyra::packer::pak, FilePackerAPI)
FORWARD_DECLARE_API(lyra::packer::zip, FilePackerAPI)

using FileLoaderPlugin = BuiltinPlugin<FileLoaderAPI>;
using FilePackerPlugin = BuiltinPlugin<FilePackerAPI>;

static constexpr uint NUM_LOADER_BACKENDS = static_cast<uint>(enum_count_v<FSLoader>);
static constexpr uint NUM_PACKER_BACKENDS = static_cast<uint>(enum_count_v<FSPacker>);

static Own<FileLoaderPlugin> loader_plugins[NUM_LOADER_BACKENDS];
static Own<FilePackerPlugin> packer_plugins[NUM_PACKER_BACKENDS];

static FileLoaderAPI* create_file_loader_api(FSLoader loader)
{
    uint index = static_cast<uint>(loader);
    if (loader_plugins[index])
        return loader_plugins[index]->get_api();

    switch (loader) {
        case FSLoader::NATIVE:
            loader_plugins[index] = std::make_unique<FileLoaderPlugin>(
                lyra::loader::native::create,
                lyra::loader::native::prepare,
                lyra::loader::native::cleanup);
            break;
        case FSLoader::PHYSFS:
            loader_plugins[index] = std::make_unique<FileLoaderPlugin>(
                lyra::loader::physfs::create,
                lyra::loader::physfs::prepare,
                lyra::loader::physfs::cleanup);
            break;
    }
    return loader_plugins[index]->get_api();
}

static FilePackerAPI* create_file_packer_api(FSPacker packer)
{
    uint index = static_cast<uint>(packer);
    if (packer_plugins[index])
        return packer_plugins[index]->get_api();

    switch (packer) {
        case FSPacker::PAK:
            packer_plugins[index] = std::make_unique<FilePackerPlugin>(
                lyra::packer::pak::create,
                lyra::packer::pak::prepare,
                lyra::packer::pak::cleanup);
            break;
        case FSPacker::ZIP:
            packer_plugins[index] = std::make_unique<FilePackerPlugin>(
                lyra::packer::zip::create,
                lyra::packer::zip::prepare,
                lyra::packer::zip::cleanup);
            break;
    }
    return packer_plugins[index]->get_api();
}

#pragma region FileLoader
FileLoader::FileLoader(FSLoader loader)
{
    api_                     = create_file_loader_api(loader);
    [[maybe_unused]] bool ok = api_->create_loader(this->loader);
    assert(ok);
}

FileLoader::FileLoader(FileLoaderAPI* api, FileLoaderHandle loader)
    : api_(api), loader(loader)
{
    // do nothing
}

void FileLoader::destroy()
{
    api_->delete_loader(loader);
    api_ = nullptr;
}

bool FileLoader::exists(FSPath vpath) const
{
    return api_->exists_file(loader, vpath);
}

usize FileLoader::size(FSPath vpath) const
{
    return api_->sizeof_file(loader, vpath);
}

Result<FileHandle> FileLoader::open(FSPath vpath) const
{
    FileHandle file;
    if (!api_->open_file(loader, file, vpath)) {
        return make_unexpected("Failed to open file: " + String(vpath ? vpath : ""));
    }
    return file;
}

void FileLoader::close(FileHandle file) const
{
    api_->close_file(loader, file);
}

Result<usize> FileLoader::read(FileHandle file, void* buffer, usize size) const
{
    usize read = 0;
    if (!api_->read_file(loader, file, buffer, size, read)) {
        return make_unexpected("Failed to read file handle");
    }
    return read;
}

Result<void> FileLoader::seek(FileHandle file, int64_t offset) const
{
    if (!api_->seek_file(loader, file, offset)) {
        return make_unexpected("Failed to seek file handle");
    }
    return {};
}

Result<MountHandle> FileLoader::mount(FSPath vpath, const Path& path, uint priority) const
{
    return mount(vpath, path.c_str(), priority);
}

Result<MountHandle> FileLoader::mount(FSPath vpath, OSPath path, uint priority) const
{
    MountHandle mount;
    if (!api_->mount(loader, mount, vpath, path, priority) || !mount.valid()) {
        return make_unexpected("Failed to mount '" + String(path ? path : "") + "' to '" + String(vpath ? vpath : "") + "'");
    }
    return mount;
}

void FileLoader::unmount(MountHandle mount) const
{
    api_->unmount(loader, mount);
}
#pragma endregion FileLoader

#pragma region FilePacker
FilePacker::FilePacker(FSPacker packer, OSPath path)
{
    api_                     = create_file_packer_api(packer);
    [[maybe_unused]] bool ok = api_->create_packer(this->packer, path);
    assert(ok);
}

FilePacker::FilePacker(FilePackerAPI* api, FilePackerHandle packer)
    : api_(api), packer(packer)
{
    // do nothing
}

void FilePacker::destroy()
{
    api_->delete_packer(packer);
    api_ = nullptr;
}

Result<void> FilePacker::write(FSPath vpath, void* buffer, usize size) const
{
    if (!api_->write(packer, vpath, buffer, size)) {
        return make_unexpected("Failed to write to archive for path: " + String(vpath ? vpath : ""));
    }
    return {};
}

Result<void> FilePacker::write(FSPath vpath, const Path& path) const
{
    // 1024B = 1kb
    // x1024 = 1Mb
    // x128  = 128Mb
    constexpr uint chunk = 1024 * 1024 * 128;

    // sanity check
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.good()) {
        spdlog::error("File {} does not exist!", path.string());
        return make_unexpected("File does not exist: " + path.string());
    }

    // get file size
    ifs.seekg(0, std::ios::end);
    usize file_size = static_cast<usize>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    // small file → one-shot read
    if (file_size < chunk) {
        Vector<char> buffer(file_size);
        ifs.read(buffer.data(), buffer.size());
        return write(vpath, buffer.data(), buffer.size());
    }

    // buffered writes
    Vector<char> buffer(chunk);
    while (ifs) {
        ifs.read(buffer.data(), buffer.size());
        std::streamsize read_bytes = ifs.gcount();
        if (read_bytes > 0) {
            auto res = write(vpath, buffer.data(), static_cast<usize>(read_bytes));
            if (!res) {
                return res;
            }
        }
    }
    return {};
}
#pragma endregion FilePacker
