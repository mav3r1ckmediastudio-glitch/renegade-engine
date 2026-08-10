#include "renegade/bridge/PackageIntegrityService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";

    int Fail(const fs::path& root, const std::string& message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool WriteFile(
        const fs::path& path,
        const std::string& text,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create package-integrity fixture directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create package-integrity fixture file";
            return false;
        }
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        if (!output)
        {
            error = "could not write package-integrity fixture file";
            return false;
        }
        return true;
    }

    bool RefreshManifest(
        const fs::path& packageRoot,
        std::string& error)
    {
        struct Record
        {
            std::string path;
            WindowsGamePackageFileDigest digest;
        };
        std::vector<Record> files;
        std::error_code ec;
        for (fs::recursive_directory_iterator iterator(packageRoot, ec), end;
             !ec && iterator != end;
             iterator.increment(ec))
        {
            if (iterator->is_directory())
                continue;
            const fs::path relative =
                fs::relative(iterator->path(), packageRoot, ec);
            if (ec)
                break;
            const std::string relativeText = relative.generic_u8string();
            if (relativeText == "package-manifest.json")
                continue;
            Record record;
            record.path = relativeText;
            if (!DigestWindowsGamePackageFile(
                    fs::absolute(iterator->path()).generic_u8string(),
                    record.digest,
                    error))
            {
                return false;
            }
            files.push_back(std::move(record));
        }
        if (ec)
        {
            error = "could not enumerate package-integrity fixture";
            return false;
        }
        std::sort(
            files.begin(),
            files.end(),
            [](const Record& left, const Record& right)
            {
                return left.path < right.path;
            });

        std::ostringstream json;
        json << "{\"format\":\"renegade-package-manifest\","
             << "\"schema_version\":1,"
             << "\"stage_only\":true,"
             << "\"distribution_ready\":false,"
             << "\"project_id\":\"" << ProjectId << "\","
             << "\"game_name\":\"Gate 4 Integrity\","
             << "\"self_path\":\"package-manifest.json\","
             << "\"self_sha256_excluded\":true,"
             << "\"files\":[";
        for (std::size_t index = 0; index < files.size(); ++index)
        {
            if (index != 0)
                json << ',';
            json << "{\"path\":\"" << files[index].path << "\","
                 << "\"bytes\":" << files[index].digest.byteCount << ','
                 << "\"sha256\":\"" << files[index].digest.sha256 << "\","
                 << "\"class\":\"gate4-test\","
                 << "\"provenance\":[\"repo:test-fixture\"]}";
        }
        json << "]}";
        return WriteFile(
            packageRoot / "package-manifest.json",
            json.str(),
            error);
    }

    bool ReadFile(
        const fs::path& path,
        std::string& text)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        text.assign(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        return input.eof();
    }
}

int main()
{
    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate4 Integrity Ω " +
            std::to_string(nonce));
    const fs::path packageRoot = root / "Moved Package With Spaces";
    const fs::path executable = packageRoot / "ProofGame.exe";
    const fs::path content = packageRoot / "GameData/Content/proof.bin";
    const fs::path readme = packageRoot / "ReadMe.txt";
    std::string error;

    if (!WriteFile(executable, "runtime-bytes\n", error) ||
        !WriteFile(content, "content-proof\n", error) ||
        !WriteFile(readme, "gate4\n", error) ||
        !RefreshManifest(packageRoot, error))
    {
        return Fail(root, error);
    }

    WindowsGamePackageIntegrityResult valid;
    if (!ValidateWindowsGamePackage(
            packageRoot.generic_u8string(),
            valid,
            error) ||
        !valid.succeeded ||
        valid.code != WindowsGamePackageIntegrityCode::Success ||
        valid.manifestFileCount != 3 ||
        valid.actualFileCount != 4 ||
        valid.packageManifestSha256.empty())
    {
        return Fail(root, error.empty()
            ? "valid exact package was rejected"
            : error);
    }

    if (!WriteFile(content, "content-PROOF\n", error))
        return Fail(root, error);
    WindowsGamePackageIntegrityResult tampered;
    if (ValidateWindowsGamePackage(
            packageRoot.generic_u8string(),
            tampered,
            error) ||
        (tampered.code != WindowsGamePackageIntegrityCode::HashMismatch &&
         tampered.code != WindowsGamePackageIntegrityCode::SizeMismatch))
    {
        return Fail(root, "tampered manifest file did not fail closed");
    }
    if (!WriteFile(content, "content-proof\n", error))
        return Fail(root, error);

    fs::remove(readme);
    WindowsGamePackageIntegrityResult missing;
    if (ValidateWindowsGamePackage(
            packageRoot.generic_u8string(),
            missing,
            error) ||
        missing.code != WindowsGamePackageIntegrityCode::MissingFile)
    {
        return Fail(root, "missing manifest file did not fail closed");
    }
    if (!WriteFile(readme, "gate4\n", error))
        return Fail(root, error);

    if (!WriteFile(packageRoot / "injected-extra.txt", "extra\n", error))
        return Fail(root, error);
    WindowsGamePackageIntegrityResult extra;
    if (ValidateWindowsGamePackage(
            packageRoot.generic_u8string(),
            extra,
            error) ||
        extra.code != WindowsGamePackageIntegrityCode::UnexpectedFile)
    {
        return Fail(root, "unmanifested package file did not fail closed");
    }
    fs::remove(packageRoot / "injected-extra.txt");

    std::string manifestText;
    if (!ReadFile(packageRoot / "package-manifest.json", manifestText))
        return Fail(root, "could not read manifest for collision fixture");
    const std::size_t filesEnd = manifestText.rfind("]}");
    if (filesEnd == std::string::npos)
        return Fail(root, "could not locate manifest file-array terminator");
    WindowsGamePackageFileDigest readmeDigest;
    if (!DigestWindowsGamePackageFile(
            fs::absolute(readme).generic_u8string(),
            readmeDigest,
            error))
    {
        return Fail(root, error);
    }
    manifestText.insert(
        filesEnd,
        ",{\"path\":\"README.TXT\",\"bytes\":" +
            std::to_string(readmeDigest.byteCount) +
            ",\"sha256\":\"" + readmeDigest.sha256 +
            "\",\"class\":\"gate4-test\","
            "\"provenance\":[\"repo:test-fixture\"]}");
    if (!WriteFile(
            packageRoot / "package-manifest.json",
            manifestText,
            error))
    {
        return Fail(root, error);
    }
    WindowsGamePackageIntegrityResult duplicate;
    if (ValidateWindowsGamePackage(
            packageRoot.generic_u8string(),
            duplicate,
            error) ||
        duplicate.code != WindowsGamePackageIntegrityCode::DuplicatePath)
    {
        return Fail(root, "Windows case-equivalent manifest collision was accepted");
    }

    std::error_code ignored;
    fs::remove_all(root, ignored);
    std::cout << "PASS: LP06 Gate 4 exact loose package integrity\n";
    return 0;
}
