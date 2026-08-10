#include "renegade/bridge/BuildIdentityService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "json.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <winver.h>
#endif

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* ApplicationManifestPolicy =
            "asInvoker+PerMonitorV2+longPathAware+utf8";

        struct FileDigest
        {
            std::uint64_t byteCount = 0;
            std::string sha256;
        };

#if defined(_WIN32)
        std::string Hex(const std::vector<unsigned char>& bytes)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');
            for (const unsigned char value : bytes)
                stream << std::setw(2) << static_cast<unsigned int>(value);
            return stream.str();
        }

        bool DigestFile(
            const fs::path& path,
            FileDigest& digest,
            std::string& error)
        {
            digest = {};
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD objectBytes = 0;
            DWORD hashBytes = 0;
            DWORD returned = 0;
            std::vector<unsigned char> object;
            std::vector<unsigned char> result;

            if (BCryptOpenAlgorithmProvider(
                    &algorithm,
                    BCRYPT_SHA256_ALGORITHM,
                    nullptr,
                    0) < 0 ||
                BCryptGetProperty(
                    algorithm,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&objectBytes),
                    sizeof(objectBytes),
                    &returned,
                    0) < 0 ||
                BCryptGetProperty(
                    algorithm,
                    BCRYPT_HASH_LENGTH,
                    reinterpret_cast<PUCHAR>(&hashBytes),
                    sizeof(hashBytes),
                    &returned,
                    0) < 0)
            {
                if (algorithm != nullptr)
                    BCryptCloseAlgorithmProvider(algorithm, 0);
                error = "Gate 3 could not initialize SHA-256.";
                return false;
            }

            object.resize(objectBytes);
            result.resize(hashBytes);
            if (BCryptCreateHash(
                    algorithm,
                    &hash,
                    object.data(),
                    objectBytes,
                    nullptr,
                    0,
                    0) < 0)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                error = "Gate 3 could not create SHA-256 state.";
                return false;
            }

            std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                BCryptDestroyHash(hash);
                BCryptCloseAlgorithmProvider(algorithm, 0);
                error = "Gate 3 could not read staged file: " +
                    path.generic_u8string();
                return false;
            }

            std::array<char, 64 * 1024> buffer{};
            bool success = true;
            while (input)
            {
                input.read(
                    buffer.data(),
                    static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count > 0)
                {
                    if (BCryptHashData(
                            hash,
                            reinterpret_cast<PUCHAR>(buffer.data()),
                            static_cast<ULONG>(count),
                            0) < 0)
                    {
                        success = false;
                        break;
                    }
                    digest.byteCount += static_cast<std::uint64_t>(count);
                }
            }
            if (!input.eof())
                success = false;

            if (success &&
                BCryptFinishHash(
                    hash,
                    result.data(),
                    hashBytes,
                    0) >= 0)
            {
                digest.sha256 = Hex(result);
            }
            else
            {
                success = false;
                error = "Gate 3 could not finish SHA-256.";
            }

            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return success;
        }
#else
        bool DigestFile(
            const fs::path&,
            FileDigest&,
            std::string& error)
        {
            error = "Gate 3 Windows hashing is only available on Windows.";
            return false;
        }
#endif

        WindowsGameStagedFile* FindStagedFile(
            WindowsGameBuildStageResult& stage,
            const std::string& destinationPath)
        {
            const auto found = std::find_if(
                stage.files.begin(),
                stage.files.end(),
                [&destinationPath](const WindowsGameStagedFile& file)
                {
                    return file.destinationPath == destinationPath;
                });
            return found == stage.files.end() ? nullptr : &*found;
        }

        const WindowsGameStagedFile* FindStagedFile(
            const WindowsGameBuildStageResult& stage,
            const std::string& destinationPath)
        {
            const auto found = std::find_if(
                stage.files.begin(),
                stage.files.end(),
                [&destinationPath](const WindowsGameStagedFile& file)
                {
                    return file.destinationPath == destinationPath;
                });
            return found == stage.files.end() ? nullptr : &*found;
        }

        bool RefreshRecord(
            WindowsGameBuildStageResult& stage,
            const std::string& destinationPath,
            std::string& error)
        {
            WindowsGameStagedFile* record =
                FindStagedFile(stage, destinationPath);
            if (record == nullptr)
            {
                error = "Gate 3 lost the staged record for: " + destinationPath;
                return false;
            }
            FileDigest digest;
            if (!DigestFile(
                    fs::u8path(stage.stagingPath) /
                        fs::u8path(destinationPath),
                    digest,
                    error))
            {
                return false;
            }
            record->byteCount = digest.byteCount;
            record->sha256 = std::move(digest.sha256);
            return true;
        }

        bool WriteTextAndRefresh(
            WindowsGameBuildStageResult& stage,
            const std::string& destinationPath,
            const std::string& text,
            std::string& error)
        {
            const fs::path path =
                fs::u8path(stage.stagingPath) / fs::u8path(destinationPath);
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error =
                    "Gate 3 could not rewrite generated package metadata: " +
                    destinationPath;
                return false;
            }
            output.write(
                text.data(),
                static_cast<std::streamsize>(text.size()));
            output.close();
            if (!output)
            {
                error =
                    "Gate 3 could not complete generated package metadata: " +
                    destinationPath;
                return false;
            }
            return RefreshRecord(stage, destinationPath, error);
        }

        nlohmann::json StagedFileJson(
            const WindowsGameStagedFile& file)
        {
            nlohmann::json item;
            item["path"] = file.destinationPath;
            item["bytes"] = file.byteCount;
            item["sha256"] = file.sha256;
            item["class"] = file.fileClass;
            item["provenance"] = file.provenance;
            return item;
        }

        bool IsIdentityText(
            const std::string& value,
            const std::size_t maximum)
        {
            return !value.empty() &&
                value.size() <= maximum &&
                value.find('\0') == std::string::npos &&
                value.find('\r') == std::string::npos &&
                value.find('\n') == std::string::npos;
        }

        bool IsBuildId(const std::string& value)
        {
            return IsIdentityText(value, 128) &&
                std::all_of(
                    value.begin(),
                    value.end(),
                    [](const unsigned char character)
                    {
                        return std::isalnum(character) != 0 ||
                            character == '-' ||
                            character == '_' ||
                            character == '.';
                    });
        }

        bool IsUtcTimestamp(const std::string& value)
        {
            if (value.size() != 20 ||
                value[4] != '-' ||
                value[7] != '-' ||
                value[10] != 'T' ||
                value[13] != ':' ||
                value[16] != ':' ||
                value[19] != 'Z')
            {
                return false;
            }
            for (std::size_t index = 0; index < value.size(); ++index)
            {
                if (index == 4 || index == 7 || index == 10 ||
                    index == 13 || index == 16 || index == 19)
                {
                    continue;
                }
                if (!std::isdigit(
                        static_cast<unsigned char>(value[index])))
                {
                    return false;
                }
            }
            return true;
        }

        bool ParseVersionPart(
            const std::string& text,
            std::uint16_t& value)
        {
            if (text.empty() ||
                !std::all_of(
                    text.begin(),
                    text.end(),
                    [](const unsigned char character)
                    {
                        return std::isdigit(character) != 0;
                    }))
            {
                return false;
            }

            std::uint32_t parsed = 0;
            for (const char character : text)
            {
                parsed = parsed * 10u +
                    static_cast<std::uint32_t>(character - '0');
                if (parsed > 65535u)
                    return false;
            }
            value = static_cast<std::uint16_t>(parsed);
            return true;
        }

        bool ParsePublicVersion(
            const std::string& publicVersion,
            std::array<std::uint16_t, 4>& parts)
        {
            parts = {};
            const std::size_t suffix =
                publicVersion.find_first_of("-+");
            const std::string core =
                publicVersion.substr(0, suffix);
            std::vector<std::string> tokens;
            std::size_t begin = 0;
            while (begin <= core.size())
            {
                const std::size_t end = core.find('.', begin);
                tokens.push_back(core.substr(
                    begin,
                    end == std::string::npos
                        ? std::string::npos
                        : end - begin));
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
            if (tokens.size() < 3 || tokens.size() > 4)
                return false;
            for (std::size_t index = 0; index < tokens.size(); ++index)
            {
                if (!ParseVersionPart(tokens[index], parts[index]))
                    return false;
            }
            return suffix == std::string::npos ||
                suffix + 1 < publicVersion.size();
        }

        std::string XmlEscape(const std::string& value)
        {
            std::string result;
            for (const char character : value)
            {
                switch (character)
                {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result.push_back(character); break;
                }
            }
            return result;
        }

        std::string BuildApplicationManifest(
            const std::string& description)
        {
            std::ostringstream stream;
            stream
                << "<?xml version=\"1.0\" encoding=\"UTF-8\" "
                   "standalone=\"yes\"?>"
                << "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" "
                   "manifestVersion=\"1.0\">"
                << "<assemblyIdentity version=\"1.0.0.0\" "
                   "processorArchitecture=\"*\" name=\"Renegade.Game\" "
                   "type=\"win32\"/>"
                << "<description>" << XmlEscape(description)
                << "</description>"
                << "<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">"
                << "<security><requestedPrivileges>"
                << "<requestedExecutionLevel level=\"asInvoker\" "
                   "uiAccess=\"false\"/>"
                << "</requestedPrivileges></security></trustInfo>"
                << "<compatibility "
                   "xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">"
                << "<application>"
                << "<supportedOS "
                   "Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>"
                << "</application></compatibility>"
                << "<application xmlns=\"urn:schemas-microsoft-com:asm.v3\">"
                << "<windowsSettings>"
                << "<dpiAware "
                   "xmlns=\"http://schemas.microsoft.com/SMI/2005/"
                   "WindowsSettings\">true/pm</dpiAware>"
                << "<dpiAwareness "
                   "xmlns=\"http://schemas.microsoft.com/SMI/2016/"
                   "WindowsSettings\">PerMonitorV2</dpiAwareness>"
                << "<longPathAware "
                   "xmlns=\"http://schemas.microsoft.com/SMI/2016/"
                   "WindowsSettings\">true</longPathAware>"
                << "<activeCodePage "
                   "xmlns=\"http://schemas.microsoft.com/SMI/2019/"
                   "WindowsSettings\">UTF-8</activeCodePage>"
                << "</windowsSettings></application>"
                << "</assembly>";
            return stream.str();
        }

#if defined(_WIN32)
        bool Utf8ToWide(
            const std::string& value,
            std::wstring& wide,
            std::string& error)
        {
            wide.clear();
            if (value.empty())
                return true;
            if (value.size() >
                static_cast<std::size_t>(
                    (std::numeric_limits<int>::max)()))
            {
                error = "Gate 3 UTF-8 identity text is too large.";
                return false;
            }
            const int required = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0);
            if (required <= 0)
            {
                error = "Gate 3 identity contains invalid UTF-8.";
                return false;
            }
            wide.resize(static_cast<std::size_t>(required));
            if (MultiByteToWideChar(
                    CP_UTF8,
                    MB_ERR_INVALID_CHARS,
                    value.data(),
                    static_cast<int>(value.size()),
                    wide.data(),
                    required) != required)
            {
                error = "Gate 3 could not decode UTF-8 identity text.";
                wide.clear();
                return false;
            }
            return true;
        }

        void AppendWord(
            std::vector<std::uint8_t>& bytes,
            const std::uint16_t value)
        {
            bytes.push_back(
                static_cast<std::uint8_t>(value & 0xffu));
            bytes.push_back(
                static_cast<std::uint8_t>((value >> 8) & 0xffu));
        }

        void AppendWideString(
            std::vector<std::uint8_t>& bytes,
            const std::wstring& value)
        {
            for (const wchar_t character : value)
            {
                AppendWord(
                    bytes,
                    static_cast<std::uint16_t>(character));
            }
            AppendWord(bytes, 0);
        }

        void AlignDword(std::vector<std::uint8_t>& bytes)
        {
            while ((bytes.size() & 3u) != 0u)
                bytes.push_back(0);
        }

        void SetWord(
            std::vector<std::uint8_t>& bytes,
            const std::size_t offset,
            const std::uint16_t value)
        {
            bytes[offset] =
                static_cast<std::uint8_t>(value & 0xffu);
            bytes[offset + 1] =
                static_cast<std::uint8_t>((value >> 8) & 0xffu);
        }

        std::size_t BeginVersionBlock(
            std::vector<std::uint8_t>& bytes,
            const std::uint16_t valueLength,
            const std::uint16_t type,
            const std::wstring& key)
        {
            const std::size_t start = bytes.size();
            AppendWord(bytes, 0);
            AppendWord(bytes, valueLength);
            AppendWord(bytes, type);
            AppendWideString(bytes, key);
            AlignDword(bytes);
            return start;
        }

        bool EndVersionBlock(
            std::vector<std::uint8_t>& bytes,
            const std::size_t start)
        {
            AlignDword(bytes);
            const std::size_t length = bytes.size() - start;
            if (length > 65535u)
                return false;
            SetWord(
                bytes,
                start,
                static_cast<std::uint16_t>(length));
            return true;
        }

        bool AddVersionString(
            std::vector<std::uint8_t>& bytes,
            const std::wstring& key,
            const std::wstring& value)
        {
            if (value.size() >= 65535u)
                return false;
            const std::size_t block = BeginVersionBlock(
                bytes,
                static_cast<std::uint16_t>(value.size() + 1u),
                1,
                key);
            AppendWideString(bytes, value);
            return EndVersionBlock(bytes, block);
        }

        bool BuildVersionResource(
            const WindowsGameBuildPlan& plan,
            const WindowsGameExecutableIdentityRequest& request,
            const std::array<std::uint16_t, 4>& version,
            std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            std::wstring gameName;
            std::wstring executableName;
            std::wstring publicVersion;
            std::wstring publisher;
            std::wstring description;
            std::wstring copyrightText;
            std::wstring buildId;
            std::wstring buildTimestamp;
            std::wstring saveDataId;
            if (!Utf8ToWide(plan.gameName, gameName, error) ||
                !Utf8ToWide(
                    plan.executableFileName, executableName, error) ||
                !Utf8ToWide(
                    plan.publicVersion, publicVersion, error) ||
                !Utf8ToWide(
                    request.developerPublisher, publisher, error) ||
                !Utf8ToWide(
                    request.description, description, error) ||
                !Utf8ToWide(
                    request.copyrightNotice, copyrightText, error) ||
                !Utf8ToWide(
                    request.internalBuildId, buildId, error) ||
                !Utf8ToWide(
                    request.buildTimestampUtc, buildTimestamp, error) ||
                !Utf8ToWide(plan.saveDataId, saveDataId, error))
            {
                return false;
            }

            bytes.clear();
            const std::size_t root = BeginVersionBlock(
                bytes,
                static_cast<std::uint16_t>(
                    sizeof(VS_FIXEDFILEINFO)),
                0,
                L"VS_VERSION_INFO");

            VS_FIXEDFILEINFO fixed{};
            fixed.dwSignature = VS_FFI_SIGNATURE;
            fixed.dwStrucVersion = VS_FFI_STRUCVERSION;
            fixed.dwFileVersionMS =
                (static_cast<std::uint32_t>(version[0]) << 16) |
                version[1];
            fixed.dwFileVersionLS =
                (static_cast<std::uint32_t>(version[2]) << 16) |
                version[3];
            fixed.dwProductVersionMS = fixed.dwFileVersionMS;
            fixed.dwProductVersionLS = fixed.dwFileVersionLS;
            fixed.dwFileFlagsMask = VS_FFI_FILEFLAGSMASK;
            fixed.dwFileFlags = 0;
            fixed.dwFileOS = VOS_NT_WINDOWS32;
            fixed.dwFileType = VFT_APP;
            const auto* fixedBytes =
                reinterpret_cast<const std::uint8_t*>(&fixed);
            bytes.insert(
                bytes.end(),
                fixedBytes,
                fixedBytes + sizeof(fixed));
            AlignDword(bytes);

            const std::size_t stringFileInfo =
                BeginVersionBlock(
                    bytes, 0, 1, L"StringFileInfo");
            const std::size_t stringTable =
                BeginVersionBlock(
                    bytes, 0, 1, L"040904B0");

            std::wstring internalName = executableName;
            const std::size_t extension =
                internalName.rfind(L'.');
            if (extension != std::wstring::npos)
                internalName.resize(extension);

            const std::vector<
                std::pair<std::wstring, std::wstring>> strings = {
                {L"CompanyName", publisher},
                {L"FileDescription", description},
                {L"FileVersion", publicVersion},
                {L"InternalName", internalName},
                {L"LegalCopyright", copyrightText},
                {L"OriginalFilename", executableName},
                {L"ProductName", gameName},
                {L"ProductVersion", publicVersion},
                {L"SpecialBuild", buildId},
                {L"RenegadeBuildTimestampUTC", buildTimestamp},
                {L"RenegadeSaveDataId", saveDataId},
            };
            for (const auto& [key, value] : strings)
            {
                if (!AddVersionString(bytes, key, value))
                {
                    error =
                        "Gate 3 Windows VERSIONINFO exceeded its "
                        "supported size.";
                    return false;
                }
            }
            if (!EndVersionBlock(bytes, stringTable) ||
                !EndVersionBlock(bytes, stringFileInfo))
            {
                error =
                    "Gate 3 Windows VERSIONINFO string table is too large.";
                return false;
            }

            const std::size_t varFileInfo =
                BeginVersionBlock(
                    bytes, 0, 1, L"VarFileInfo");
            const std::size_t translation =
                BeginVersionBlock(
                    bytes, 4, 0, L"Translation");
            AppendWord(bytes, 0x0409u);
            AppendWord(bytes, 1200u);
            if (!EndVersionBlock(bytes, translation) ||
                !EndVersionBlock(bytes, varFileInfo) ||
                !EndVersionBlock(bytes, root))
            {
                error = "Gate 3 Windows VERSIONINFO is too large.";
                return false;
            }
            return true;
        }

        std::uint16_t ReadLe16(
            const std::vector<std::uint8_t>& bytes,
            const std::size_t offset)
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[offset]) |
                (static_cast<std::uint16_t>(
                    bytes[offset + 1]) << 8));
        }

        std::uint32_t ReadLe32(
            const std::vector<std::uint8_t>& bytes,
            const std::size_t offset)
        {
            return static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(
                    bytes[offset + 1]) << 8) |
                (static_cast<std::uint32_t>(
                    bytes[offset + 2]) << 16) |
                (static_cast<std::uint32_t>(
                    bytes[offset + 3]) << 24);
        }

        bool ReadIcon(
            const fs::path& iconPath,
            std::vector<std::vector<std::uint8_t>>& images,
            std::vector<std::uint8_t>& group,
            std::string& error)
        {
            std::error_code ec;
            if (!iconPath.is_absolute() ||
                fs::is_symlink(
                    fs::symlink_status(iconPath, ec)) ||
                ec ||
                !fs::is_regular_file(iconPath, ec) ||
                ec)
            {
                error =
                    "Gate 3 icon input must be an absolute "
                    "non-symlink .ico file.";
                return false;
            }

            std::ifstream input(iconPath, std::ios::binary);
            if (!input)
            {
                error = "Gate 3 could not read the governed .ico input.";
                return false;
            }
            std::vector<std::uint8_t> iconBytes(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
            if (!input.eof() && input.fail())
            {
                error = "Gate 3 could not read the governed .ico input.";
                return false;
            }
            if (iconBytes.size() < 6 ||
                ReadLe16(iconBytes, 0) != 0 ||
                ReadLe16(iconBytes, 2) != 1)
            {
                error =
                    "Gate 3 icon input is not a Windows ICO file.";
                return false;
            }

            const std::uint16_t count =
                ReadLe16(iconBytes, 4);
            if (count == 0 ||
                count > 64 ||
                iconBytes.size() <
                    6u + static_cast<std::size_t>(count) * 16u)
            {
                error =
                    "Gate 3 icon input has an invalid image table.";
                return false;
            }

            group.clear();
            AppendWord(group, 0);
            AppendWord(group, 1);
            AppendWord(group, count);
            images.clear();
            images.reserve(count);
            for (std::uint16_t index = 0;
                 index < count;
                 ++index)
            {
                const std::size_t entry =
                    6u + static_cast<std::size_t>(index) * 16u;
                const std::uint32_t size =
                    ReadLe32(iconBytes, entry + 8u);
                const std::uint32_t offset =
                    ReadLe32(iconBytes, entry + 12u);
                if (size == 0 ||
                    static_cast<std::uint64_t>(offset) + size >
                        iconBytes.size())
                {
                    error =
                        "Gate 3 icon input contains an "
                        "out-of-range image.";
                    return false;
                }

                group.insert(
                    group.end(),
                    iconBytes.begin() +
                        static_cast<std::ptrdiff_t>(entry),
                    iconBytes.begin() +
                        static_cast<std::ptrdiff_t>(entry + 12u));
                AppendWord(
                    group,
                    static_cast<std::uint16_t>(index + 1u));
                images.emplace_back(
                    iconBytes.begin() +
                        static_cast<std::ptrdiff_t>(offset),
                    iconBytes.begin() +
                        static_cast<std::ptrdiff_t>(offset + size));
            }
            return true;
        }

        bool UpdateResourceBytes(
            HANDLE updater,
            LPCWSTR type,
            const std::uint16_t id,
            const WORD language,
            const std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            if (bytes.size() >
                static_cast<std::size_t>(
                    (std::numeric_limits<DWORD>::max)()))
            {
                error = "Gate 3 resource input is too large.";
                return false;
            }
            if (!UpdateResourceW(
                    updater,
                    type,
                    MAKEINTRESOURCEW(id),
                    language,
                    const_cast<std::uint8_t*>(bytes.data()),
                    static_cast<DWORD>(bytes.size())))
            {
                error =
                    "Gate 3 UpdateResourceW failed with Win32 error " +
                    std::to_string(GetLastError()) + ".";
                return false;
            }
            return true;
        }

        bool StampExecutableResources(
            const fs::path& executablePath,
            const WindowsGameBuildPlan& plan,
            const WindowsGameExecutableIdentityRequest& request,
            const std::array<std::uint16_t, 4>& version,
            std::string& error)
        {
            std::vector<std::uint8_t> versionBytes;
            if (!BuildVersionResource(
                    plan,
                    request,
                    version,
                    versionBytes,
                    error))
            {
                return false;
            }

            const std::string manifestText =
                BuildApplicationManifest(request.description);
            const std::vector<std::uint8_t> manifestBytes(
                manifestText.begin(),
                manifestText.end());

            std::vector<std::vector<std::uint8_t>> iconImages;
            std::vector<std::uint8_t> iconGroup;
            if (!ReadIcon(
                    fs::u8path(request.iconSourcePath),
                    iconImages,
                    iconGroup,
                    error))
            {
                return false;
            }

            HANDLE updater = BeginUpdateResourceW(
                executablePath.c_str(),
                FALSE);
            if (updater == nullptr)
            {
                error =
                    "Gate 3 could not open the staged executable "
                    "resource table; Win32 error " +
                    std::to_string(GetLastError()) + ".";
                return false;
            }

            constexpr WORD EnglishUs =
                MAKELANGID(
                    LANG_ENGLISH,
                    SUBLANG_ENGLISH_US);
            constexpr WORD Neutral =
                MAKELANGID(
                    LANG_NEUTRAL,
                    SUBLANG_NEUTRAL);

            bool success = UpdateResourceBytes(
                updater,
                RT_VERSION,
                1,
                EnglishUs,
                versionBytes,
                error);
            if (success)
            {
                success = UpdateResourceBytes(
                    updater,
                    RT_MANIFEST,
                    1,
                    EnglishUs,
                    manifestBytes,
                    error);
            }
            if (success)
            {
                success = UpdateResourceBytes(
                    updater,
                    RT_MANIFEST,
                    1,
                    Neutral,
                    manifestBytes,
                    error);
            }
            for (std::size_t index = 0;
                 success && index < iconImages.size();
                 ++index)
            {
                success = UpdateResourceBytes(
                    updater,
                    RT_ICON,
                    static_cast<std::uint16_t>(index + 1u),
                    Neutral,
                    iconImages[index],
                    error);
            }
            if (success)
            {
                success = UpdateResourceBytes(
                    updater,
                    RT_GROUP_ICON,
                    1,
                    Neutral,
                    iconGroup,
                    error);
            }

            if (!EndUpdateResourceW(
                    updater,
                    success ? FALSE : TRUE))
            {
                if (success)
                {
                    error =
                        "Gate 3 could not commit executable resources; "
                        "Win32 error " +
                        std::to_string(GetLastError()) + ".";
                }
                return false;
            }
            return success;
        }
#endif

        bool ParseJson(
            const std::string& text,
            const char* subject,
            nlohmann::json& document,
            std::string& error)
        {
            try
            {
                document = nlohmann::json::parse(text);
                if (!document.is_object())
                {
                    error = std::string("Gate 3 ") + subject +
                        " is not a JSON object.";
                    return false;
                }
                return true;
            }
            catch (const std::exception& exception)
            {
                error =
                    std::string("Gate 3 could not parse ") +
                    subject + ": " + exception.what();
                return false;
            }
        }

        bool ValidateManifestIdentity(
            const nlohmann::json& projectManifest,
            const WindowsGameBuildPlan& plan,
            std::string& error)
        {
            if (projectManifest.value(
                    "format", std::string{}) !=
                    "renegade-project-package-manifest" ||
                projectManifest.value(
                    "project_id", std::string{}) != plan.projectId ||
                projectManifest.value(
                    "game_name", std::string{}) != plan.gameName ||
                projectManifest.value(
                    "executable", std::string{}) !=
                    plan.executableFileName ||
                projectManifest.value(
                    "public_version", std::string{}) !=
                    plan.publicVersion ||
                projectManifest.value(
                    "save_data_id", std::string{}) !=
                    plan.saveDataId)
            {
                error =
                    "Gate 3 project manifest identity no longer agrees "
                    "with the accepted Gate 1 plan.";
                return false;
            }
            return true;
        }

        void AddIdentityFields(
            nlohmann::json& document,
            const WindowsGameBuildPlan& plan,
            const WindowsGameExecutableIdentityRequest& request)
        {
            document["public_version"] = plan.publicVersion;
            document["save_data_id"] = plan.saveDataId;
            document["developer_publisher"] =
                request.developerPublisher;
            document["description"] = request.description;
            document["copyright"] =
                request.copyrightNotice;
            document["internal_build_id"] =
                request.internalBuildId;
            document["build_timestamp_utc"] =
                request.buildTimestampUtc;
            document["application_manifest_policy"] =
                ApplicationManifestPolicy;
            document["icon_resource"] = true;
        }

        bool RewriteGate3Manifests(
            const WindowsGameBuildPlan& plan,
            const WindowsGameExecutableIdentityRequest& request,
            WindowsGameBuildStageResult& stage,
            std::string& error)
        {
            nlohmann::json projectManifest;
            nlohmann::json runtimeManifest;
            if (!ParseJson(
                    stage.projectManifestJson,
                    "project package manifest",
                    projectManifest,
                    error) ||
                !ParseJson(
                    stage.runtimeSupportManifestJson,
                    "Runtime support manifest",
                    runtimeManifest,
                    error) ||
                !ValidateManifestIdentity(
                    projectManifest, plan, error))
            {
                return false;
            }

            const WindowsGameStagedFile* executable =
                FindStagedFile(
                    stage, plan.executableFileName);
            if (executable == nullptr)
            {
                error =
                    "Gate 3 staged executable record is missing.";
                return false;
            }

            projectManifest["schema_version"] = 2;
            projectManifest["bootstrap_mode"] =
                "package_relative";
            projectManifest["stage_only"] = true;
            AddIdentityFields(projectManifest, plan, request);
            stage.projectManifestJson =
                projectManifest.dump();
            if (!WriteTextAndRefresh(
                    stage,
                    "GameData/project.manifest.json",
                    stage.projectManifestJson,
                    error))
            {
                return false;
            }
            stage.projectManifestSha256 =
                FindStagedFile(
                    stage,
                    "GameData/project.manifest.json")->sha256;

            runtimeManifest["schema_version"] = 2;
            AddIdentityFields(runtimeManifest, plan, request);
            bool foundExecutable = false;
            if (!runtimeManifest.contains("files") ||
                !runtimeManifest["files"].is_array())
            {
                error =
                    "Gate 3 Runtime support manifest has no file array.";
                return false;
            }
            for (auto& item : runtimeManifest["files"])
            {
                if (item.value("path", std::string{}) ==
                    plan.executableFileName)
                {
                    item["bytes"] = executable->byteCount;
                    item["sha256"] = executable->sha256;
                    item["identity_stamped"] = true;
                    foundExecutable = true;
                }
            }
            if (!foundExecutable)
            {
                error =
                    "Gate 3 Runtime support manifest does not describe "
                    "the named executable.";
                return false;
            }
            stage.runtimeSupportManifestJson =
                runtimeManifest.dump();
            if (!WriteTextAndRefresh(
                    stage,
                    "Engine/runtime-support-manifest.json",
                    stage.runtimeSupportManifestJson,
                    error))
            {
                return false;
            }
            stage.runtimeSupportManifestSha256 =
                FindStagedFile(
                    stage,
                    "Engine/runtime-support-manifest.json")->sha256;

            nlohmann::json buildReport;
            const fs::path buildReportPath =
                fs::u8path(stage.stagingPath) /
                "build-report.json";
            {
                std::ifstream input(
                    buildReportPath,
                    std::ios::binary);
                if (!input)
                {
                    error =
                        "Gate 3 build report is missing.";
                    return false;
                }
                try
                {
                    input >> buildReport;
                }
                catch (const std::exception& exception)
                {
                    error =
                        std::string(
                            "Gate 3 could not parse build report: ") +
                        exception.what();
                    return false;
                }
            }
            if (!buildReport.is_object())
            {
                error =
                    "Gate 3 build report is not a JSON object.";
                return false;
            }
            buildReport["schema_version"] = 2;
            buildReport["status"] =
                "identity_applied_not_launched";
            buildReport["stage_only"] = true;
            buildReport["distribution_ready"] = false;
            buildReport["smoke_test"] =
                "not_run_gate3";
            buildReport["promotion"] =
                "not_attempted_gate3";
            AddIdentityFields(buildReport, plan, request);
            if (!WriteTextAndRefresh(
                    stage,
                    "build-report.json",
                    buildReport.dump(),
                    error))
            {
                return false;
            }

            nlohmann::json packageManifest;
            packageManifest["format"] =
                "renegade-package-manifest";
            packageManifest["schema_version"] = 2;
            packageManifest["stage_only"] = true;
            packageManifest["distribution_ready"] = false;
            packageManifest["project_id"] = plan.projectId;
            packageManifest["game_name"] = plan.gameName;
            packageManifest["executable"] =
                plan.executableFileName;
            packageManifest["platform"] = plan.platform;
            packageManifest["configuration"] =
                plan.configuration;
            packageManifest["project_manifest_sha256"] =
                stage.projectManifestSha256;
            packageManifest["content_manifest_sha256"] =
                stage.contentManifestSha256;
            packageManifest[
                "runtime_support_manifest_sha256"] =
                stage.runtimeSupportManifestSha256;
            packageManifest["self_path"] =
                "package-manifest.json";
            packageManifest["self_sha256_excluded"] = true;
            AddIdentityFields(packageManifest, plan, request);
            packageManifest["files"] =
                nlohmann::json::array();

            std::vector<WindowsGameStagedFile> ordered =
                stage.files;
            std::sort(
                ordered.begin(),
                ordered.end(),
                [](const WindowsGameStagedFile& left,
                   const WindowsGameStagedFile& right)
                {
                    return left.destinationPath <
                        right.destinationPath;
                });
            for (const WindowsGameStagedFile& file : ordered)
            {
                if (file.destinationPath ==
                    "package-manifest.json")
                {
                    continue;
                }
                packageManifest["files"].push_back(
                    StagedFileJson(file));
            }

            stage.packageManifestJson =
                packageManifest.dump();
            if (!WriteTextAndRefresh(
                    stage,
                    "package-manifest.json",
                    stage.packageManifestJson,
                    error))
            {
                return false;
            }
            stage.packageManifestSha256 =
                FindStagedFile(
                    stage,
                    "package-manifest.json")->sha256;
            return true;
        }
    }

    bool ApplyWindowsGameExecutableIdentity(
        const WindowsGameBuildPlan& plan,
        const WindowsGameExecutableIdentityRequest& request,
        WindowsGameBuildStageResult& stage,
        WindowsGameExecutableIdentityResult& result,
        std::string& error)
    {
        result = {};
        error.clear();

        std::string canonicalPlan;
        if (!SerializeWindowsGameBuildPlan(
                plan, canonicalPlan, error))
        {
            return false;
        }
        (void)canonicalPlan;

        if (plan.platform != "windows-x64" ||
            plan.configuration != "Release")
        {
            error =
                "Gate 3 executable identity requires the Windows x64 "
                "Release build plan.";
            return false;
        }
        if (!IsIdentityText(
                request.developerPublisher, 256) ||
            !IsIdentityText(
                request.description, 512) ||
            !IsIdentityText(
                request.copyrightNotice, 512) ||
            !IsBuildId(request.internalBuildId) ||
            !IsUtcTimestamp(request.buildTimestampUtc))
        {
            error =
                "Gate 3 executable identity metadata is invalid.";
            return false;
        }

        std::array<std::uint16_t, 4> version{};
        if (!ParsePublicVersion(
                plan.publicVersion, version))
        {
            error =
                "Gate 3 public version must begin with a three- or "
                "four-part numeric Windows version.";
            return false;
        }

        if (!ValidateWindowsGameBuildStage(stage, error))
            return false;
        if (stage.stagingPath.empty() ||
            stage.finalOutputPath.empty())
        {
            error =
                "Gate 3 requires a complete Gate 2 staging result.";
            return false;
        }

        WindowsGameStagedFile* executable =
            FindStagedFile(
                stage, plan.executableFileName);
        if (executable == nullptr ||
            executable->fileClass != "runtime_support")
        {
            error =
                "Gate 3 could not identify the staged named Runtime.";
            return false;
        }

        const fs::path executablePath =
            fs::u8path(stage.stagingPath) /
            fs::u8path(plan.executableFileName);
        std::error_code ec;
        if (fs::is_symlink(
                fs::symlink_status(executablePath, ec)) ||
            ec ||
            !fs::is_regular_file(executablePath, ec) ||
            ec)
        {
            error =
                "Gate 3 named executable is missing or symlinked.";
            return false;
        }

#if defined(_WIN32)
        if (!StampExecutableResources(
                executablePath,
                plan,
                request,
                version,
                error))
        {
            return false;
        }
#else
        error =
            "Gate 3 Windows executable identity is only "
            "available on Windows.";
        return false;
#endif

        if (!RefreshRecord(
                stage, plan.executableFileName, error))
        {
            return false;
        }
        executable =
            FindStagedFile(
                stage, plan.executableFileName);
        if (executable == nullptr)
        {
            error =
                "Gate 3 lost the executable record after "
                "resource stamping.";
            return false;
        }
        if (std::find(
                executable->provenance.begin(),
                executable->provenance.end(),
                "lp06:gate3:identity-stamped") ==
            executable->provenance.end())
        {
            executable->provenance.push_back(
                "lp06:gate3:identity-stamped");
            std::sort(
                executable->provenance.begin(),
                executable->provenance.end());
        }

        if (!RewriteGate3Manifests(
                plan, request, stage, error))
        {
            return false;
        }
        if (!ValidateWindowsGameBuildStage(stage, error))
            return false;

        result.executableSha256 =
            FindStagedFile(
                stage, plan.executableFileName)->sha256;
        result.applicationManifestPolicy =
            ApplicationManifestPolicy;
        error.clear();
        return true;
    }
}
