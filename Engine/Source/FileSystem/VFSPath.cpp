#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/Utilities/Collections.h>

#include <algorithm>
#include <vector>

using namespace lyra;

static String normalize_subpath(StringView raw_path)
{
    if (raw_path.empty()) return "";

    String s(raw_path);
    std::replace(s.begin(), s.end(), '\\', '/');

    // split into segments and normalize '.' and '..'
    Vector<String> segments;
    usize          start = 0;
    while (start < s.size()) {
        usize end = s.find('/', start);
        if (end == String::npos) end = s.size();

        String segment = s.substr(start, end - start);
        if (!segment.empty() && segment != ".") {
            if (segment == "..") {
                if (!segments.empty()) {
                    segments.pop_back();
                }
            } else {
                segments.push_back(std::move(segment));
            }
        }
        start = end + 1;
    }

    // join segments
    String result;
    for (usize i = 0; i < segments.size(); ++i) {
        if (i > 0) result += '/';
        result += segments[i];
    }
    return result;
}

VFSPath::VFSPath(StringView scheme, StringView path)
    : scheme(scheme), path(normalize_subpath(path))
{
}

bool VFSPath::has_protocol(StringView raw_vpath)
{
    auto pos = raw_vpath.find("://");
    return pos != StringView::npos && pos > 0;
}

VFSPath VFSPath::parse(StringView raw_vpath)
{
    if (raw_vpath.empty()) {
        return VFSPath{};
    }

    auto pos = raw_vpath.find("://");
    if (pos == StringView::npos || pos == 0) {
        // scheme is strictly required; bare paths without scheme are invalid
        return VFSPath{};
    }

    StringView scheme_part = raw_vpath.substr(0, pos);
    StringView path_part   = raw_vpath.substr(pos + 3);
    return VFSPath(scheme_part, path_part);
}

String VFSPath::to_string() const
{
    if (scheme.empty()) {
        return "";
    }
    if (path.empty()) {
        return scheme + "://";
    }
    return scheme + "://" + path;
}
