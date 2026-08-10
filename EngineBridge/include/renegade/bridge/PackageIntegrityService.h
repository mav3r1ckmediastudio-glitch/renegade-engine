#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace renegade::bridge
{
    enum class WindowsGamePackageIntegrityCode
    {
        Success,
        InvalidRoot,
        MissingManifest,
        InvalidManifest,
        UnsafePath,
        DuplicatePath,
        MissingFile,
        SymlinkRejected,
        SizeMismatch,
        HashMismatch,
        UnexpectedFile,
    };

    struct WindowsGamePackageFileDigest
    {
        std::uint64_t byteCount = 0;
        std::string sha256;
    };

    struct WindowsGamePackageIntegrityResult
    {
        bool succeeded = false;
        WindowsGamePackageIntegrityCode code =
            WindowsGamePackageIntegrityCode::InvalidRoot;
        std::string message;
        std::string packageRootPath;
        std::string projectId;
        std::string gameName;
        std::string packageManifestSha256;
        std::size_t manifestFileCount = 0;
        std::size_t actualFileCount = 0;
    };

    [[nodiscard]] const char* WindowsGamePackageIntegrityCodeName(
        WindowsGamePackageIntegrityCode code) noexcept;

    // Computes the package SHA-256 contract using Windows BCrypt. The path must
    // identify a regular, non-symlink file. This utility is intentionally
    // exposed so Gate 4 verification/report code and deterministic package test
    // fixtures use the exact same digest semantics as Runtime validation.
    [[nodiscard]] bool DigestWindowsGamePackageFile(
        const std::string& absolutePath,
        WindowsGamePackageFileDigest& digest,
        std::string& error);

    // Re-enumerates a loose Windows package from package-manifest.json and
    // fails closed on missing/extra/symlinked/case-colliding/tampered files.
    // package-manifest.json itself is required but is intentionally excluded
    // from its self-referential file array, matching the accepted Gate 2/3
    // manifest contract.
    [[nodiscard]] bool ValidateWindowsGamePackage(
        const std::string& packageRootPath,
        WindowsGamePackageIntegrityResult& result,
        std::string& error);
}
