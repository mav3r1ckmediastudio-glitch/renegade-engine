#pragma once
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

namespace renegade::bridge
{
    // The destination first receives the staged companions, then any remaining
    // terrain products. Overwriting an existing companion adds only its growth.
    inline bool CanFitTestLevelSnapshot(std::uintmax_t available, std::uintmax_t archiveBytes,
        const std::filesystem::path& session, const std::filesystem::path& source,
        const std::set<std::string>& terrainFiles)
    {
        namespace fs = std::filesystem;
        constexpr std::uintmax_t reserve = 4ull * 1024 * 1024 * 1024;
        if (available <= reserve || archiveBytes >= available - reserve) return false;
        auto remaining = available - reserve - archiveBytes;
        auto consume = [&](std::uintmax_t bytes) {
            if (bytes >= remaining) return false;
            remaining -= bytes;
            return true;
        };

        std::error_code ec;
        fs::recursive_directory_iterator it(session, ec), end;
        while (!ec && it != end)
        {
            if (it->is_regular_file(ec) && !consume(it->file_size(ec))) return false;
            if (ec) return false;
            it.increment(ec);
        }
        if (ec) return false;
        for (const auto& file : terrainFiles)
        {
            const auto relative = fs::u8path(file);
            const auto incoming = fs::file_size(source / relative, ec);
            if (ec) return false;
            const auto existing = session / relative;
            const bool staged = fs::exists(existing, ec);
            if (ec) return false;
            const auto original = staged ? fs::file_size(existing, ec) : 0;
            if (ec || (incoming > original && !consume(incoming - original))) return false;
        }
        return true;
    }
}
