#include "SlangFileSystem.h"
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace lyra;

SlangBlobImpl::SlangBlobImpl(Vector<char> data)
    : data_(std::move(data))
{
    data_.push_back('\0');
}

SlangBlobImpl::SlangBlobImpl(const void* data, size_t size)
{
    if (data && size > 0) {
        data_.assign(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size);
    }
    data_.push_back('\0');
}

SlangBlobImpl::SlangBlobImpl(StringView text)
{
    data_.assign(text.begin(), text.end());
    data_.push_back('\0');
}

ISlangUnknown* SlangBlobImpl::getInterface(const SlangUUID& uuid)
{
    if (uuid == ISlangUnknown::getTypeGuid() || uuid == ISlangBlob::getTypeGuid()) {
        return static_cast<ISlangBlob*>(this);
    }
    return nullptr;
}

const void* SlangBlobImpl::getBufferPointer()
{
    return data_.empty() ? "" : data_.data();
}

size_t SlangBlobImpl::getBufferSize()
{
    return data_.empty() ? 0 : data_.size() - 1;
}

ComPtr<ISlangBlob> make_blob(const void* data, size_t size)
{
    return ComPtr<ISlangBlob>(new SlangBlobImpl(data, size));
}

ComPtr<ISlangBlob> make_blob(StringView text)
{
    return ComPtr<ISlangBlob>(new SlangBlobImpl(text));
}

ComPtr<ISlangBlob> make_blob(Vector<char> data)
{
    return ComPtr<ISlangBlob>(new SlangBlobImpl(std::move(data)));
}

SlangVFSFileSystem::SlangVFSFileSystem(FileLoader* loader, const Vector<String>& search_dirs)
    : loader_(loader), search_dirs_(search_dirs)
{
}

ISlangUnknown* SlangVFSFileSystem::getInterface(const SlangUUID& uuid)
{
    if (uuid == ISlangUnknown::getTypeGuid() ||
        uuid == ISlangCastable::getTypeGuid() ||
        uuid == ISlangFileSystem::getTypeGuid() ||
        uuid == ISlangFileSystemExt::getTypeGuid()) {
        return static_cast<ISlangFileSystemExt*>(this);
    }
    return nullptr;
}

void* SlangVFSFileSystem::castAs(const SlangUUID& guid)
{
    return getInterface(guid);
}

SlangResult SlangVFSFileSystem::loadFile(const char* path, ISlangBlob** outBlob)
{
    if (!path || !outBlob) return SLANG_E_INVALID_ARG;
    *outBlob = nullptr;

    // 1. check if it uses protocol based path
    if (loader_ && VFSPath::has_protocol(path)) {
        auto data = loader_->read<char>(path);
        if (!data.empty()) {
            *outBlob = make_blob(std::move(data)).detach();
            return SLANG_OK;
        }
        return SLANG_E_NOT_FOUND;
    }

    // 2. fallback to OS file system search
    std::error_code       ec;
    std::filesystem::path p(path);
    if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
        std::ifstream file(p, std::ios::binary);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            auto str = buffer.str();
            *outBlob = make_blob(str.data(), str.size()).detach();
            return SLANG_OK;
        }
    }

    for (const auto& sp : search_dirs_) {
        auto candidate = std::filesystem::path(sp) / p;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            std::ifstream file(candidate, std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                auto str = buffer.str();
                *outBlob = make_blob(str.data(), str.size()).detach();
                return SLANG_OK;
            }
        }
    }

    return SLANG_E_NOT_FOUND;
}

SlangResult SlangVFSFileSystem::calcCombinedPath(
    SlangPathType fromPathType,
    const char*   fromPath,
    const char*   path,
    ISlangBlob**  pathOut)
{
    if (!path || !pathOut) return SLANG_E_INVALID_ARG;
    *pathOut = nullptr;

    String path_str(path);

    // if target path is already an absolute protocol URI, return it directly
    if (loader_ && VFSPath::has_protocol(path_str)) {
        *pathOut = make_blob(path_str).detach();
        return SLANG_OK;
    }

    // if fromPath has protocol, combine target relative path with fromPath's VFS location
    if (loader_ && fromPath && VFSPath::has_protocol(fromPath)) {
        VFSPath               from = VFSPath::parse(fromPath);
        std::filesystem::path from_sub(from.path);

        std::filesystem::path dir;
        if (fromPathType == SLANG_PATH_TYPE_DIRECTORY) {
            dir = from_sub;
        } else {
            dir = from_sub.parent_path();
        }

        std::filesystem::path combined   = dir.empty() ? std::filesystem::path(path) : (dir / path);
        String                normalized = combined.lexically_normal().generic_string();

        while (!normalized.empty() && (normalized.front() == '/' || normalized.front() == '\\')) {
            normalized.erase(0, 1);
        }

        String result = from.scheme + "://" + normalized;
        *pathOut      = make_blob(result).detach();
        return SLANG_OK;
    }

    // fallback to normal OS filesystem combining
    std::filesystem::path from_p = fromPath ? std::filesystem::path(fromPath) : std::filesystem::path();
    std::filesystem::path p(path);

    std::filesystem::path combined;
    if (p.is_absolute()) {
        combined = p;
    } else if (from_p.empty()) {
        combined = p;
    } else if (fromPathType == SLANG_PATH_TYPE_DIRECTORY) {
        combined = from_p / p;
    } else {
        combined = from_p.parent_path() / p;
    }

    String result = combined.lexically_normal().generic_string();
    *pathOut      = make_blob(result).detach();
    return SLANG_OK;
}

SlangResult SlangVFSFileSystem::getFileUniqueIdentity(
    const char*  path,
    ISlangBlob** outUniqueIdentity)
{
    if (!path || !outUniqueIdentity) return SLANG_E_INVALID_ARG;
    *outUniqueIdentity = nullptr;

    if (loader_ && VFSPath::has_protocol(path)) {
        VFSPath parsed = VFSPath::parse(path);
        if (parsed.is_valid()) {
            std::filesystem::path p(parsed.path);
            String                norm = p.lexically_normal().generic_string();
            while (!norm.empty() && (norm.front() == '/' || norm.front() == '\\')) {
                norm.erase(0, 1);
            }
            String canonical_vpath = parsed.scheme + "://" + norm;
            *outUniqueIdentity     = make_blob(canonical_vpath).detach();
            return SLANG_OK;
        }
    }

    std::error_code ec;
    auto            canonical_p = std::filesystem::weakly_canonical(path, ec);
    String          ident       = ec ? String(path) : canonical_p.generic_string();
    *outUniqueIdentity          = make_blob(ident).detach();
    return SLANG_OK;
}

SlangResult SlangVFSFileSystem::getPathType(
    const char*    path,
    SlangPathType* pathTypeOut)
{
    if (!path || !pathTypeOut) return SLANG_E_INVALID_ARG;

    if (loader_ && VFSPath::has_protocol(path)) {
        if (loader_->exists(path)) {
            *pathTypeOut = SLANG_PATH_TYPE_FILE;
            return SLANG_OK;
        }
        return SLANG_E_NOT_FOUND;
    }

    std::error_code       ec;
    std::filesystem::path p(path);
    auto                  status = std::filesystem::status(p, ec);
    if (!ec && std::filesystem::exists(status)) {
        if (std::filesystem::is_directory(status)) {
            *pathTypeOut = SLANG_PATH_TYPE_DIRECTORY;
        } else {
            *pathTypeOut = SLANG_PATH_TYPE_FILE;
        }
        return SLANG_OK;
    }

    return SLANG_E_NOT_FOUND;
}

SlangResult SlangVFSFileSystem::getPath(
    ::PathKind   kind,
    const char*  path,
    ISlangBlob** outPath)
{
    if (!path || !outPath) return SLANG_E_INVALID_ARG;
    *outPath = make_blob(path, std::strlen(path)).detach();
    return SLANG_OK;
}

void SlangVFSFileSystem::clearCache()
{
}

SlangResult SlangVFSFileSystem::enumeratePathContents(
    const char*                path,
    FileSystemContentsCallBack callback,
    void*                      userData)
{
    return SLANG_E_NOT_IMPLEMENTED;
}

::OSPathKind SlangVFSFileSystem::getOSPathKind()
{
    return ::OSPathKind::None;
}
