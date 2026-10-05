#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace renegade::bridge::detail
{
    // Extended paths belong only at local I/O boundaries, never in manifests.
    inline std::filesystem::path FileIoPath(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        std::error_code ec;
        auto absolute = std::filesystem::absolute(path, ec);
        if (ec)
            return path;
        absolute = absolute.lexically_normal();
        absolute.make_preferred();
        const std::wstring native = absolute.native();
        if (native.rfind(L"\\\\?\\", 0) == 0)
            return absolute;
        if (native.rfind(L"\\\\", 0) == 0)
            return std::filesystem::path(L"\\\\?\\UNC\\" + native.substr(2));
        return std::filesystem::path(L"\\\\?\\" + native);
#else
        return path;
#endif
    }
}
