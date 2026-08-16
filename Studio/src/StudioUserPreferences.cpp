#include "StudioUserPreferences.h"

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <vector>

namespace
{
    constexpr char DeveloperIdentityPrefix[] = "developer_identity=";

    std::string WideToUtf8(const std::wstring_view value)
    {
        if (value.empty())
            return {};

        const int required = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (required <= 0)
            return {};

        std::string output(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                output.data(),
                required,
                nullptr,
                nullptr) <= 0)
        {
            return {};
        }
        return output;
    }

    std::wstring Utf8ToWide(const std::string_view value)
    {
        if (value.empty())
            return {};

        const int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (required <= 0)
            return {};

        std::wstring output(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                output.data(),
                required) <= 0)
        {
            return {};
        }
        return output;
    }
}

namespace renegade::studio
{
    std::filesystem::path StudioUserPreferences::PreferencesPath()
    {
        const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required == 0)
            return {};

        std::vector<wchar_t> buffer(static_cast<std::size_t>(required));
        const DWORD written = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            buffer.data(),
            required);
        if (written == 0 || written >= required)
            return {};

        return std::filesystem::path(buffer.data()) /
            L"RenegadeStudio" /
            L"preferences.cfg";
    }

    std::optional<std::wstring> StudioUserPreferences::LoadDeveloperIdentity()
    {
        const auto path = PreferencesPath();
        if (path.empty())
            return std::nullopt;

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return std::nullopt;

        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.starts_with(DeveloperIdentityPrefix))
                continue;

            const std::string_view encoded(
                line.data() + (sizeof(DeveloperIdentityPrefix) - 1),
                line.size() - (sizeof(DeveloperIdentityPrefix) - 1));
            const std::wstring decoded = Utf8ToWide(encoded);
            std::wstring normalized;
            if (!NormalizeDeveloperIdentity(decoded, normalized))
                return std::nullopt;
            return normalized;
        }

        return std::nullopt;
    }

    bool StudioUserPreferences::SaveDeveloperIdentity(
        const std::wstring_view identity)
    {
        std::wstring normalized;
        if (!NormalizeDeveloperIdentity(identity, normalized))
            return false;

        const std::string encoded = WideToUtf8(normalized);
        if (encoded.empty())
            return false;

        const auto path = PreferencesPath();
        if (path.empty())
            return false;

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return false;

        auto temporaryPath = path;
        temporaryPath += L".tmp";

        {
            std::ofstream stream(
                temporaryPath,
                std::ios::binary | std::ios::out | std::ios::trunc);
            if (!stream)
                return false;

            stream << "version=1\n";
            stream << DeveloperIdentityPrefix << encoded << '\n';
            stream.flush();
            if (!stream)
                return false;
        }

        if (!MoveFileExW(
                temporaryPath.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(temporaryPath, error);
            return false;
        }

        return true;
    }

    bool StudioUserPreferences::ClearDeveloperIdentity()
    {
        const auto path = PreferencesPath();
        if (path.empty())
            return false;

        std::error_code error;
        if (!std::filesystem::exists(path, error))
            return !error;
        return std::filesystem::remove(path, error) && !error;
    }

    bool StudioUserPreferences::NormalizeDeveloperIdentity(
        const std::wstring_view identity,
        std::wstring& normalized)
    {
        std::size_t first = 0;
        while (first < identity.size() && std::iswspace(identity[first]))
            ++first;

        std::size_t last = identity.size();
        while (last > first && std::iswspace(identity[last - 1]))
            --last;

        if (first == last)
            return false;

        const auto trimmed = identity.substr(first, last - first);
        if (trimmed.size() > MaxDeveloperIdentityCharacters)
            return false;

        for (const wchar_t character : trimmed)
        {
            if (character < 0x20 || character == 0x7F)
                return false;
        }

        normalized.assign(trimmed);
        return true;
    }
}
