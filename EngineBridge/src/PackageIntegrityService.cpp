#include "renegade/bridge/PackageIntegrityService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        struct ManifestFile
        {
            std::string path;
            std::uint64_t byteCount = 0;
            std::string sha256;
        };

        WindowsGamePackageIntegrityResult Fail(
            WindowsGamePackageIntegrityResult result,
            const WindowsGamePackageIntegrityCode code,
            std::string message,
            std::string& error)
        {
            result.succeeded = false;
            result.code = code;
            result.message = std::move(message);
            error = result.message;
            return result;
        }

        bool TryDecodeUtf8(const std::string& value, std::wstring& decoded)
        {
            decoded.clear();
            if (value.empty())
                return true;
            if (value.size() > static_cast<std::size_t>(
                    (std::numeric_limits<int>::max)()))
            {
                return false;
            }
            const int size = static_cast<int>(value.size());
            const int required = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                size,
                nullptr,
                0);
            if (required <= 0)
                return false;
            decoded.resize(static_cast<std::size_t>(required));
            return MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                size,
                decoded.data(),
                required) == required;
        }

        bool WindowsPathCaseEquivalent(
            const std::string& left,
            const std::string& right)
        {
            if (left == right)
                return true;
            std::wstring decodedLeft;
            std::wstring decodedRight;
            if (!TryDecodeUtf8(left, decodedLeft) ||
                !TryDecodeUtf8(right, decodedRight))
            {
                return false;
            }
            return CompareStringOrdinal(
                decodedLeft.data(),
                static_cast<int>(decodedLeft.size()),
                decodedRight.data(),
                static_cast<int>(decodedRight.size()),
                TRUE) == CSTR_EQUAL;
        }

        bool IsSafeRelativePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.empty() || path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
            {
                return false;
            }
            return std::none_of(
                path.begin(),
                path.end(),
                [](const fs::path& component)
                {
                    return component.empty() ||
                        component == "." || component == "..";
                });
        }

        bool IsLowerSha256(const std::string& value)
        {
            return value.size() == 64 &&
                std::all_of(
                    value.begin(),
                    value.end(),
                    [](const unsigned char character)
                    {
                        return (character >= '0' && character <= '9') ||
                            (character >= 'a' && character <= 'f');
                    });
        }

        bool ContainsPath(
            const fs::path& root,
            const fs::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() ||
                    !WindowsPathCaseEquivalent(
                        rootPart->generic_u8string(),
                        candidatePart->generic_u8string()))
                {
                    return false;
                }
            }
            return true;
        }

        bool HasSymlinkComponent(
            const fs::path& root,
            const fs::path& relative,
            std::string& offending)
        {
            fs::path cursor = root;
            std::error_code ec;
            for (const fs::path& component : relative)
            {
                cursor /= component;
                const fs::file_status status = fs::symlink_status(cursor, ec);
                if (ec)
                {
                    offending = relative.generic_u8string();
                    return true;
                }
                if (fs::is_symlink(status))
                {
                    offending = fs::relative(cursor, root, ec).generic_u8string();
                    if (ec)
                        offending = relative.generic_u8string();
                    return true;
                }
            }
            return false;
        }

        std::string Hex(const std::vector<unsigned char>& bytes)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');
            for (const unsigned char value : bytes)
            {
                stream << std::setw(2)
                       << static_cast<unsigned int>(value);
            }
            return stream.str();
        }

        const ManifestFile* FindManifestFile(
            const std::vector<ManifestFile>& files,
            const std::string& path)
        {
            const auto found = std::find_if(
                files.begin(),
                files.end(),
                [&path](const ManifestFile& file)
                {
                    return WindowsPathCaseEquivalent(file.path, path);
                });
            return found == files.end() ? nullptr : &*found;
        }
    }

    const char* WindowsGamePackageIntegrityCodeName(
        const WindowsGamePackageIntegrityCode code) noexcept
    {
        switch (code)
        {
        case WindowsGamePackageIntegrityCode::Success:
            return "SUCCESS";
        case WindowsGamePackageIntegrityCode::InvalidRoot:
            return "INVALID_ROOT";
        case WindowsGamePackageIntegrityCode::MissingManifest:
            return "MISSING_MANIFEST";
        case WindowsGamePackageIntegrityCode::InvalidManifest:
            return "INVALID_MANIFEST";
        case WindowsGamePackageIntegrityCode::UnsafePath:
            return "UNSAFE_PATH";
        case WindowsGamePackageIntegrityCode::DuplicatePath:
            return "DUPLICATE_PATH";
        case WindowsGamePackageIntegrityCode::MissingFile:
            return "MISSING_FILE";
        case WindowsGamePackageIntegrityCode::SymlinkRejected:
            return "SYMLINK_REJECTED";
        case WindowsGamePackageIntegrityCode::SizeMismatch:
            return "SIZE_MISMATCH";
        case WindowsGamePackageIntegrityCode::HashMismatch:
            return "HASH_MISMATCH";
        case WindowsGamePackageIntegrityCode::UnexpectedFile:
            return "UNEXPECTED_FILE";
        default:
            return "UNKNOWN";
        }
    }

    bool DigestWindowsGamePackageFile(
        const std::string& absolutePath,
        WindowsGamePackageFileDigest& digest,
        std::string& error)
    {
        digest = {};
        const fs::path path = fs::u8path(absolutePath);
        std::error_code ec;
        if (absolutePath.empty() || !path.is_absolute())
        {
            error = "Package digest path must be absolute.";
            return false;
        }
        const fs::file_status linkStatus = fs::symlink_status(path, ec);
        if (ec || fs::is_symlink(linkStatus) ||
            !fs::is_regular_file(path, ec) || ec)
        {
            error = "Package digest input must be a regular non-symlink file: " +
                path.generic_u8string();
            return false;
        }

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
            error = "Could not initialize package SHA-256.";
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
            error = "Could not create package SHA-256 state.";
            return false;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            error = "Could not read package file: " + path.generic_u8string();
            return false;
        }

        std::array<char, 64 * 1024> buffer{};
        bool success = true;
        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
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
            BCryptFinishHash(hash, result.data(), hashBytes, 0) >= 0)
        {
            digest.sha256 = Hex(result);
        }
        else
        {
            success = false;
            error = "Could not finish package SHA-256.";
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (success)
            error.clear();
        return success;
    }

    bool ValidateWindowsGamePackage(
        const std::string& packageRootPath,
        WindowsGamePackageIntegrityResult& result,
        std::string& error)
    {
        result = {};
        try
        {
            std::error_code ec;
            const fs::path declaredRoot = fs::u8path(packageRootPath);
            const fs::path root = fs::weakly_canonical(declaredRoot, ec);
            if (packageRootPath.empty() || ec || root.empty() ||
                !fs::is_directory(root, ec) || ec)
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidRoot,
                    "Windows game package root is not an accessible directory.",
                    error);
                return false;
            }
            result.packageRootPath = root.generic_u8string();

            const fs::path manifestPath = root / "package-manifest.json";
            const fs::file_status manifestLinkStatus =
                fs::symlink_status(manifestPath, ec);
            if (ec || fs::is_symlink(manifestLinkStatus) ||
                !fs::is_regular_file(manifestPath, ec) || ec)
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::MissingManifest,
                    "Windows game package is missing a regular package-manifest.json.",
                    error);
                return false;
            }

            nlohmann::json manifest;
            try
            {
                std::ifstream input(manifestPath, std::ios::binary);
                input >> manifest;
            }
            catch (const std::exception& exception)
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidManifest,
                    std::string("Windows game package manifest is invalid JSON: ") +
                        exception.what(),
                    error);
                return false;
            }

            if (!manifest.is_object() ||
                manifest.value("format", std::string{}) !=
                    "renegade-package-manifest" ||
                manifest.value("schema_version", 0) != 1 ||
                manifest.value("self_path", std::string{}) !=
                    "package-manifest.json" ||
                manifest.value("self_sha256_excluded", false) != true ||
                !manifest.contains("files") || !manifest["files"].is_array())
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidManifest,
                    "Windows game package manifest does not match the accepted Gate 2/3 schema.",
                    error);
                return false;
            }

            result.projectId = manifest.value("project_id", std::string{});
            result.gameName = manifest.value("game_name", std::string{});
            if (result.projectId.empty() || result.gameName.empty())
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidManifest,
                    "Windows game package manifest is missing package identity.",
                    error);
                return false;
            }

            std::vector<ManifestFile> files;
            files.reserve(manifest["files"].size());
            for (const auto& item : manifest["files"])
            {
                if (!item.is_object() ||
                    !item.contains("path") || !item["path"].is_string() ||
                    !item.contains("bytes") || !item["bytes"].is_number_unsigned() ||
                    !item.contains("sha256") || !item["sha256"].is_string())
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::InvalidManifest,
                        "Windows game package manifest contains a malformed file record.",
                        error);
                    return false;
                }

                ManifestFile file;
                file.path = item["path"].get<std::string>();
                file.byteCount = item["bytes"].get<std::uint64_t>();
                file.sha256 = item["sha256"].get<std::string>();
                if (!IsSafeRelativePath(file.path) ||
                    WindowsPathCaseEquivalent(
                        file.path,
                        "package-manifest.json"))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::UnsafePath,
                        "Windows game package manifest contains an unsafe file path: " +
                            file.path,
                        error);
                    return false;
                }
                if (!IsLowerSha256(file.sha256))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::InvalidManifest,
                        "Windows game package manifest contains an invalid SHA-256: " +
                            file.path,
                        error);
                    return false;
                }
                if (FindManifestFile(files, file.path) != nullptr)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::DuplicatePath,
                        "Windows game package manifest contains a Windows case-equivalent duplicate: " +
                            file.path,
                        error);
                    return false;
                }
                files.push_back(std::move(file));
            }
            result.manifestFileCount = files.size();

            for (const ManifestFile& file : files)
            {
                const fs::path relative = fs::u8path(file.path);
                std::string symlink;
                if (HasSymlinkComponent(root, relative, symlink))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::SymlinkRejected,
                        "Windows game package refuses symlinked content: " + symlink,
                        error);
                    return false;
                }

                const fs::path resolved =
                    fs::weakly_canonical(root / relative, ec);
                if (ec || !ContainsPath(root, resolved) ||
                    !fs::is_regular_file(resolved, ec) || ec)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::MissingFile,
                        "Windows game package is missing a manifest file: " + file.path,
                        error);
                    return false;
                }

                WindowsGamePackageFileDigest digest;
                if (!DigestWindowsGamePackageFile(
                        resolved.generic_u8string(),
                        digest,
                        error))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::MissingFile,
                        error,
                        error);
                    return false;
                }
                if (digest.byteCount != file.byteCount)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::SizeMismatch,
                        "Windows game package file size does not match the manifest: " +
                            file.path,
                        error);
                    return false;
                }
                if (digest.sha256 != file.sha256)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::HashMismatch,
                        "Windows game package file SHA-256 does not match the manifest: " +
                            file.path,
                        error);
                    return false;
                }
            }

            std::size_t actualFiles = 0;
            fs::recursive_directory_iterator iterator(
                root,
                fs::directory_options::none,
                ec);
            const fs::recursive_directory_iterator end;
            if (ec)
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidRoot,
                    "Windows game package could not be enumerated.",
                    error);
                return false;
            }
            for (; iterator != end; iterator.increment(ec))
            {
                if (ec)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::InvalidRoot,
                        "Windows game package enumeration failed.",
                        error);
                    return false;
                }

                const fs::file_status status =
                    fs::symlink_status(iterator->path(), ec);
                if (ec || fs::is_symlink(status))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::SymlinkRejected,
                        "Windows game package contains a symlink.",
                        error);
                    return false;
                }
                if (fs::is_directory(status))
                    continue;
                if (!fs::is_regular_file(status))
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::UnexpectedFile,
                        "Windows game package contains a non-regular filesystem entry.",
                        error);
                    return false;
                }

                ++actualFiles;
                const fs::path relative = fs::relative(iterator->path(), root, ec);
                if (ec)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::UnexpectedFile,
                        "Windows game package could not normalize an enumerated path.",
                        error);
                    return false;
                }
                const std::string relativeText = relative.generic_u8string();
                if (WindowsPathCaseEquivalent(
                        relativeText,
                        "package-manifest.json"))
                {
                    continue;
                }
                if (FindManifestFile(files, relativeText) == nullptr)
                {
                    result = Fail(
                        std::move(result),
                        WindowsGamePackageIntegrityCode::UnexpectedFile,
                        "Windows game package contains an unmanifested file: " +
                            relativeText,
                        error);
                    return false;
                }
            }

            result.actualFileCount = actualFiles;
            if (actualFiles != files.size() + 1u)
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::UnexpectedFile,
                    "Windows game package file count does not match its manifest.",
                    error);
                return false;
            }

            WindowsGamePackageFileDigest manifestDigest;
            if (!DigestWindowsGamePackageFile(
                    manifestPath.generic_u8string(),
                    manifestDigest,
                    error))
            {
                result = Fail(
                    std::move(result),
                    WindowsGamePackageIntegrityCode::InvalidManifest,
                    error,
                    error);
                return false;
            }

            result.succeeded = true;
            result.code = WindowsGamePackageIntegrityCode::Success;
            result.message = "Windows game package matches package-manifest.json exactly.";
            result.packageManifestSha256 = std::move(manifestDigest.sha256);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            result = Fail(
                std::move(result),
                WindowsGamePackageIntegrityCode::InvalidManifest,
                std::string("Windows game package validation failed: ") +
                    exception.what(),
                error);
            return false;
        }
    }
}
