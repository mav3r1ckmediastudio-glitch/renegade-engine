#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace renegade::studio
{
    class StudioUserPreferences final
    {
    public:
        static constexpr std::size_t MaxDeveloperIdentityCharacters = 32;

        [[nodiscard]] static std::filesystem::path PreferencesPath();
        [[nodiscard]] static std::optional<std::wstring> LoadDeveloperIdentity();
        [[nodiscard]] static bool SaveDeveloperIdentity(std::wstring_view identity);
        [[nodiscard]] static bool ClearDeveloperIdentity();
        [[nodiscard]] static bool NormalizeDeveloperIdentity(
            std::wstring_view identity,
            std::wstring& normalized);
    };
}
