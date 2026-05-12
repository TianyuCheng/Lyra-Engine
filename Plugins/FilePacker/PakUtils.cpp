#include <fstream>
#include <cstring>
#include <algorithm>
#include "PakUtils.h"

using namespace lyra;
using namespace lyra::file_packer::pak;

static Logger get_shared_logger()
{
    static Logger logger = create_logger("FilePacker", LogLevel::trace);
    return logger;
}

Logger lyra::file_packer::pak::get_logger()
{
    return get_shared_logger();
}

// normalize path for PAK format: convert to forward slashes, no leading slash, max 55 chars
String PakArchive::normalize_path(FSPath vpath)
{
    if (!vpath) return String("");

    String s(vpath);
    if (s.empty()) return String("");

    // replace backslashes with forward slashes
    std::replace(s.begin(), s.end(), '\\', '/');

    // remove leading '/'
    while (!s.empty() && s.front() == '/')
        s.erase(0, 1);

    // PAK format has a 56-byte filename field (55 chars + null terminator max)
    if (s.length() > 55) {
        get_logger()->warn("normalize_pak_path: truncating path '{}' to 55 characters", s);
        s = s.substr(0, 55);
    }

    return s;
}

// write data in little-endian format
void PakArchive::write_le32(std::ostream& os, uint value)
{
    uint8_t bytes[4];
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    os.write(reinterpret_cast<const char*>(bytes), 4);
}

// finalize and write the PAK file to disk
bool PakArchive::finalize()
{
    if (is_finalized) {
        return true; // already finalized
    }

    std::ofstream file(archive_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        get_logger()->error("finalize_pak_archive: failed to open {} for writing", archive_path.string());
        return false;
    }

    // write placeholder header (we'll update it later)
    PakHeader header;
    std::memcpy(header.signature, "PACK", 4);
    header.dir_offset = 0;
    header.dir_size   = 0;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    uint current_offset = sizeof(PakHeader);

    // write file data and track offsets
    using FileInfo = std::pair<uint, uint>;        // (offset, size)
    Vector<std::pair<String, FileInfo>> file_info; // filename -> (offset, size)

    for (const auto& entry : files) {
        uint file_offset = current_offset;
        uint file_size   = static_cast<uint>(entry.data.size());

        // write file data
        file.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());

        file_info.emplace_back(entry.filename, std::make_pair(file_offset, file_size));
        current_offset += file_size;

        get_logger()->trace("finalize_pak_archive: wrote file '{}' at offset {} (size {})",
            entry.filename, file_offset, file_size);
    }

    // write directory
    uint dir_offset = current_offset;
    uint dir_size   = static_cast<uint>(file_info.size() * sizeof(PakDirEntry));

    for (const auto& info : file_info) {
        PakDirEntry dir_entry;
        std::memset(&dir_entry, 0, sizeof(dir_entry));

        // copy filename (ensure null termination)
        std::strncpy(dir_entry.filename, info.first.c_str(), 55);
        dir_entry.filename[55] = '\0';

        // write offset and size in little-endian
        dir_entry.offset = info.second.first;
        dir_entry.size   = info.second.second;

        // convert to little-endian for cross-platform compatibility
        uint8_t* offset_bytes = reinterpret_cast<uint8_t*>(&dir_entry.offset);
        uint8_t* size_bytes   = reinterpret_cast<uint8_t*>(&dir_entry.size);

        // ensure little-endian byte order
        uint offset_le = info.second.first;
        uint size_le   = info.second.second;

        offset_bytes[0] = static_cast<uint8_t>(offset_le & 0xFF);
        offset_bytes[1] = static_cast<uint8_t>((offset_le >> 8) & 0xFF);
        offset_bytes[2] = static_cast<uint8_t>((offset_le >> 16) & 0xFF);
        offset_bytes[3] = static_cast<uint8_t>((offset_le >> 24) & 0xFF);

        size_bytes[0] = static_cast<uint8_t>(size_le & 0xFF);
        size_bytes[1] = static_cast<uint8_t>((size_le >> 8) & 0xFF);
        size_bytes[2] = static_cast<uint8_t>((size_le >> 16) & 0xFF);
        size_bytes[3] = static_cast<uint8_t>((size_le >> 24) & 0xFF);

        file.write(reinterpret_cast<const char*>(&dir_entry), sizeof(dir_entry));
    }

    // update header with directory info
    file.seekp(0);

    // write signature
    file.write("PACK", 4);

    // write directory offset and size in little-endian
    write_le32(file, dir_offset);
    write_le32(file, dir_size);

    if (!file.good()) {
        get_logger()->error("finalize_pak_archive: failed to write PAK file {}", archive_path.string());
        return false;
    }

    is_finalized = true;
    get_logger()->info("finalize_pak_archive: successfully wrote PAK file {} with {} files",
        archive_path.string(), files.size());
    return true;
}
