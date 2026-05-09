#include <fstream>

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Assert.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/FileIO/VFSTypes.h>

using namespace lyra;

using FileLoaderPlugin = Plugin<FileLoaderAPI>;
using FilePackerPlugin = Plugin<FilePackerAPI>;

static constexpr uint NUM_LOADER_BACKENDS = static_cast<uint>(magic_enum::enum_count<FSLoader>());
static constexpr uint NUM_PACKER_BACKENDS = static_cast<uint>(magic_enum::enum_count<FSPacker>());

static Own<FileLoaderPlugin> loader_plugins[NUM_LOADER_BACKENDS];
static Own<FilePackerPlugin> packer_plugins[NUM_PACKER_BACKENDS];

static FileLoaderAPI* create_file_loader_api(FSLoader loader)
{
    uint index = static_cast<uint>(loader);
    if (loader_plugins[index])
        return loader_plugins[index]->get_api();

    switch (loader) {
        case FSLoader::NATIVE:
            loader_plugins[index] = std::make_unique<FileLoaderPlugin>("lyra-nativefs");
            break;
        case FSLoader::PHYSFS:
            loader_plugins[index] = std::make_unique<FileLoaderPlugin>("lyra-physfs");
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
            packer_plugins[index] = std::make_unique<FilePackerPlugin>("lyra-pak");
            break;
        case FSPacker::ZIP:
            packer_plugins[index] = std::make_unique<FilePackerPlugin>("lyra-zip");
            break;
    }
    return packer_plugins[index]->get_api();
}

#pragma region FileLoader
FileLoader::FileLoader(FSLoader loader)
{
    api_ = create_file_loader_api(loader);
    assert(api_->create_loader(this->loader));
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

size_t FileLoader::size(FSPath vpath) const
{
    return api_->sizeof_file(loader, vpath);
}

FileHandle FileLoader::open(FSPath vpath) const
{
    FileHandle file;
    assert(api_->open_file(loader, file, vpath));
    return file;
}

void FileLoader::close(FileHandle file) const
{
    api_->close_file(loader, file);
}

size_t FileLoader::read(FileHandle file, void* buffer, size_t size) const
{
    size_t read = 0;
    assert(api_->read_file(loader, file, buffer, size, read));
    return read;
}

void FileLoader::seek(FileHandle file, int64_t offset) const
{
    assert(api_->seek_file(loader, file, offset));
}

MountHandle FileLoader::mount(FSPath vpath, const Path& path, uint priority) const
{
    MountHandle mount;
    assert(api_->mount(loader, mount, vpath, path.c_str(), priority));
    return mount;
}

MountHandle FileLoader::mount(FSPath vpath, OSPath path, uint priority) const
{
    MountHandle mount;
    assert(api_->mount(loader, mount, vpath, path, priority));
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
    api_ = create_file_packer_api(packer);
    assert(api_->create_packer(this->packer, path));
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

void FilePacker::write(FSPath vpath, void* buffer, size_t size) const
{
    api_->write(packer, vpath, buffer, size);
}

void FilePacker::write(FSPath vpath, const Path& path) const
{
    // 1024B = 1kb
    // x1024 = 1Mb
    // x128  = 128Mb
    constexpr uint chunk = 1024 * 1024 * 128;

    // sanity check
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.good()) {
        spdlog::error("File {} does not exist!", path.string());
        return;
    }

    // get file size
    ifs.seekg(0, std::ios::end);
    std::size_t file_size = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    // small file → one-shot read
    if (file_size < chunk) {
        Vector<char> buffer(file_size);
        ifs.read(buffer.data(), buffer.size());
        write(vpath, buffer.data(), buffer.size());
        return;
    }

    // buffered writes
    Vector<char> buffer(chunk);
    while (ifs) {
        ifs.read(buffer.data(), buffer.size());
        std::streamsize read_bytes = ifs.gcount();
        if (read_bytes > 0)
            write(vpath, buffer.data(), static_cast<std::size_t>(read_bytes));
    }
}
#pragma endregion FilePacker
