#pragma once

#ifndef LYRA_MODULE_COMPILER_SLANG_FILESYSTEM_H
#define LYRA_MODULE_COMPILER_SLANG_FILESYSTEM_H

#include "SlangCommon.h"
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <atomic>

using namespace lyra;

class SlangBlobImpl : public ISlangBlob
{
public:
    explicit SlangBlobImpl(Vector<char> data);
    explicit SlangBlobImpl(const void* data, size_t size);
    explicit SlangBlobImpl(StringView text);

    ISlangUnknown* getInterface(const SlangUUID& uuid);

    SLANG_IUNKNOWN_ALL

    SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() override;
    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override;

private:
    std::atomic<uint32_t> m_refCount{0};
    Vector<char>          data_;
};

auto make_blob(const void* data, size_t size) -> ComPtr<ISlangBlob>;
auto make_blob(StringView text) -> ComPtr<ISlangBlob>;
auto make_blob(Vector<char> data) -> ComPtr<ISlangBlob>;

class SlangVFSFileSystem : public ISlangFileSystemExt
{
public:
    explicit SlangVFSFileSystem(FileLoader* loader, const Vector<String>& search_dirs);

    ISlangUnknown* getInterface(const SlangUUID& uuid);

    SLANG_IUNKNOWN_ALL

    SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID& guid) override;

    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const* path, ISlangBlob** outBlob) override;

    SLANG_NO_THROW SlangResult SLANG_MCALL calcCombinedPath(
        SlangPathType fromPathType,
        const char*   fromPath,
        const char*   path,
        ISlangBlob**  pathOut) override;

    SLANG_NO_THROW SlangResult SLANG_MCALL getFileUniqueIdentity(
        const char*  path,
        ISlangBlob** outUniqueIdentity) override;

    SLANG_NO_THROW SlangResult SLANG_MCALL getPathType(
        const char*    path,
        SlangPathType* pathTypeOut) override;

    SLANG_NO_THROW SlangResult SLANG_MCALL getPath(
        ::PathKind   kind,
        const char*  path,
        ISlangBlob** outPath) override;

    SLANG_NO_THROW void SLANG_MCALL clearCache() override;

    SLANG_NO_THROW SlangResult SLANG_MCALL enumeratePathContents(
        const char*                path,
        FileSystemContentsCallBack callback,
        void*                      userData) override;

    SLANG_NO_THROW ::OSPathKind SLANG_MCALL getOSPathKind() override;

private:
    std::atomic<uint32_t> m_refCount{0};
    FileLoader*           loader_ = nullptr;
    Vector<String>        search_dirs_;
};

#endif // LYRA_MODULE_COMPILER_SLANG_FILESYSTEM_H
