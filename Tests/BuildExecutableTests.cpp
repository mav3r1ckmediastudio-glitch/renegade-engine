#include "renegade/bridge/BuildIdentityService.h"

#include <Windows.h>
#include <bcrypt.h>
#include <winver.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* SaveDataId =
        "22222222-2222-4222-8222-222222222222";

    struct Digest
    {
        std::uint64_t bytes = 0;
        std::string sha256;
    };

    int Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    std::string Hex(const std::vector<unsigned char>& bytes)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (const unsigned char value : bytes)
            stream << std::setw(2) << static_cast<unsigned int>(value);
        return stream.str();
    }

    bool Sha256File(
        const fs::path& path,
        Digest& digest,
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
            error = "could not initialize SHA-256";
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
            error = "could not create SHA-256 hash";
            return false;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            error = "could not open SHA-256 input";
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
                digest.bytes += static_cast<std::uint64_t>(count);
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
            error = "could not finish SHA-256";
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return success;
    }

    std::string Fnv1a64(const std::string& contents)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char value : contents)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool WriteFile(
        const fs::path& path,
        const std::string& contents,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create fixture directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create fixture file";
            return false;
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output)
        {
            error = "could not write fixture file";
            return false;
        }
        return true;
    }

    void AppendWord(
        std::vector<std::uint8_t>& bytes,
        const std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    }

    void AppendDword(
        std::vector<std::uint8_t>& bytes,
        const std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    }

    bool WriteIcon(
        const fs::path& path,
        std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        AppendWord(bytes, 0);
        AppendWord(bytes, 1);
        AppendWord(bytes, 1);
        bytes.push_back(1);
        bytes.push_back(1);
        bytes.push_back(0);
        bytes.push_back(0);
        AppendWord(bytes, 1);
        AppendWord(bytes, 32);
        AppendDword(bytes, 48);
        AppendDword(bytes, 22);
        AppendDword(bytes, 40);
        AppendDword(bytes, 1);
        AppendDword(bytes, 2);
        AppendWord(bytes, 1);
        AppendWord(bytes, 32);
        AppendDword(bytes, 0);
        AppendDword(bytes, 4);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        AppendDword(bytes, 0);
        bytes.push_back(0x20);
        bytes.push_back(0x80);
        bytes.push_back(0xff);
        bytes.push_back(0xff);
        AppendDword(bytes, 0);

        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create icon directory";
            return false;
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create icon fixture";
            return false;
        }
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
        {
            error = "could not write icon fixture";
            return false;
        }
        return true;
    }

    WindowsGameBuildFile ProjectFile(
        const std::string& contents)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::ProjectContent;
        file.destinationPath = "GameData/ProofGame.renegade";
        file.projectRelativeSourcePath = "ProofGame.renegade";
        file.assetId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
        file.dependencyClass = DependencyClass::ProjectDocument;
        file.requirement = DependencyRequirement::Required;
        file.sourceContentHash = Fnv1a64(contents);
        file.provenance = {"lp05:gate3-identity-fixture"};
        return file;
    }

    WindowsGameBuildFile RuntimeFile(
        std::string destination,
        std::string supportName,
        const Digest& digest)
    {
        WindowsGameBuildFile file;
        file.kind = WindowsGameBuildFileKind::RuntimeSupport;
        file.destinationPath = std::move(destination);
        file.runtimeSupportName = std::move(supportName);
        file.byteCount = digest.bytes;
        file.sha256 = digest.sha256;
        file.provenance = {"lp06:gate3:test-runtime"};
        return file;
    }

    WindowsGameBuildPlan Plan(
        const std::string& projectText,
        const Digest& runtimeDigest,
        const Digest& dxcDigest)
    {
        WindowsGameBuildPlan plan;
        plan.projectId = ProjectId;
        plan.gameName = "Proof Game";
        plan.executableFileName = "ProofGame.exe";
        plan.buildFolderName = "Proof Game Windows Build";
        plan.publicVersion = "0.1.0-gate3";
        plan.saveDataId = SaveDataId;
        plan.files = {
            ProjectFile(projectText),
            RuntimeFile(
                "ProofGame.exe",
                "renegade-runtime",
                runtimeDigest),
            RuntimeFile(
                "dxcompiler.dll",
                "directx-shader-compiler",
                dxcDigest),
        };
        return plan;
    }

    std::vector<WindowsPackageDocumentInput> Documents(
        const fs::path& fixtureRoot)
    {
        const std::string readme =
            (fixtureRoot / "ReadMe.txt").generic_u8string();
        const std::string legal =
            (fixtureRoot / "Fixture-Legal-Notice.txt").generic_u8string();
        return {
            {"ReadMe.txt", readme, "gate3-test-readme",
                "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/Renegade-Licence-or-Notice.txt", legal,
                "renegade-test-policy", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/WickedEngine-LICENSE.txt", legal,
                "wicked-test-notice", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/WickedEngine-third_party_software.txt", legal,
                "wicked-third-party-test-notice",
                "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/DirectXShaderCompiler-LICENSE.txt", legal,
                "dxc-test-notice", "repo:test-fixture:lp06-gate2-v1"},
            {"Licences/DirectXShaderCompiler-ThirdPartyNotices.txt", legal,
                "dxc-third-party-test-notice",
                "repo:test-fixture:lp06-gate2-v1"},
        };
    }

    WindowsGameBuildStagingRequest StageRequest(
        const fs::path& projectRoot,
        const fs::path& outputRoot,
        const fs::path& runtimePath,
        const fs::path& dxcPath,
        const fs::path& fixtureRoot,
        std::string stagingId)
    {
        WindowsGameBuildStagingRequest request;
        request.projectRootPath = projectRoot.generic_u8string();
        request.outputParentPath = outputRoot.generic_u8string();
        request.stagingId = std::move(stagingId);
        request.renegadeRevision =
            "cc83e5f111800cbf82f4bc01bcaf15dae988187a";
        request.wickedRevision =
            "3a800b7134aafe58461093c8abb2e274d4e64033";
        request.runtimeSupportSources = {
            {"ProofGame.exe", runtimePath.generic_u8string()},
            {"dxcompiler.dll", dxcPath.generic_u8string()},
        };
        request.packageDocuments = Documents(fixtureRoot);
        return request;
    }

    WindowsGameExecutableIdentityRequest IdentityRequest(
        const fs::path& icon)
    {
        WindowsGameExecutableIdentityRequest request;
        request.developerPublisher = "Maverick Media Studio";
        request.description = "Proof Game standalone";
        request.copyrightNotice = "Copyright 2026 Maverick Media Studio";
        request.internalBuildId = "cc83e5f-gate3-proof";
        request.buildTimestampUtc = "2026-08-10T00:00:00Z";
        request.iconSourcePath = icon.generic_u8string();
        return request;
    }

    std::wstring VersionString(
        const fs::path& executable,
        const wchar_t* key,
        std::string& error)
    {
        DWORD ignored = 0;
        const DWORD size =
            GetFileVersionInfoSizeW(executable.c_str(), &ignored);
        if (size == 0)
        {
            error = "stamped executable has no VERSIONINFO";
            return {};
        }
        std::vector<std::uint8_t> bytes(size);
        if (!GetFileVersionInfoW(
                executable.c_str(),
                0,
                size,
                bytes.data()))
        {
            error = "could not read stamped VERSIONINFO";
            return {};
        }

        std::wstring query = L"\\StringFileInfo\\040904B0\\";
        query += key;
        LPVOID value = nullptr;
        UINT length = 0;
        if (!VerQueryValueW(
                bytes.data(),
                query.c_str(),
                &value,
                &length) ||
            value == nullptr ||
            length == 0)
        {
            error = "missing stamped VERSIONINFO field";
            return {};
        }
        return std::wstring(
            static_cast<const wchar_t*>(value),
            static_cast<std::size_t>(length - 1));
    }

    std::string ResourceText(
        const fs::path& executable,
        LPCWSTR type,
        const std::uint16_t id,
        std::string& error)
    {
        HMODULE module = LoadLibraryExW(
            executable.c_str(),
            nullptr,
            LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (module == nullptr)
        {
            error = "could not open stamped PE resources";
            return {};
        }
        HRSRC resource = FindResourceW(
            module,
            MAKEINTRESOURCEW(id),
            type);
        if (resource == nullptr)
        {
            FreeLibrary(module);
            error = "required stamped PE resource is missing";
            return {};
        }
        const DWORD size = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        const void* data = loaded == nullptr ? nullptr : LockResource(loaded);
        if (data == nullptr || size == 0)
        {
            FreeLibrary(module);
            error = "required stamped PE resource could not be read";
            return {};
        }
        const std::string text(
            static_cast<const char*>(data),
            static_cast<std::size_t>(size));
        FreeLibrary(module);
        return text;
    }

    bool HasResource(
        const fs::path& executable,
        LPCWSTR type,
        const std::uint16_t id)
    {
        HMODULE module = LoadLibraryExW(
            executable.c_str(),
            nullptr,
            LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (module == nullptr)
            return false;
        const bool found = FindResourceW(
            module,
            MAKEINTRESOURCEW(id),
            type) != nullptr;
        FreeLibrary(module);
        return found;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    if (argc != 2)
        return Fail("expected the Gate 2 package-document fixture directory");

    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate3 Identity Ω " +
            std::to_string(nonce));
    const fs::path projectRoot = root / "Project With Spaces";
    const fs::path outputRoot = root / "Build Output";
    const fs::path supportRoot = root / "Runtime Support";
    const fs::path fixtureRoot = fs::absolute(fs::u8path(argv[1]));
    const fs::path sourceRuntime = fs::absolute(fs::u8path(argv[0]));
    const fs::path dxcPath = supportRoot / "dxcompiler.dll";
    const fs::path iconPath = supportRoot / "ProofGame.ico";
    const std::string projectText =
        "format = renegade-project\n"
        "version = 1\n\n"
        "[project]\n"
        "project_id = 11111111-1111-4111-8111-111111111111\n"
        "name = Proof Game\n"
        "startup_scene = Content/Scenes/Main.wiscene\n";
    std::string error;

    if (!WriteFile(
            projectRoot / "ProofGame.renegade",
            projectText,
            error) ||
        !WriteFile(dxcPath, "gate3-dxc-fixture\n", error) ||
        !WriteIcon(iconPath, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    Digest runtimeBefore;
    Digest dxcDigest;
    if (!Sha256File(sourceRuntime, runtimeBefore, error) ||
        !Sha256File(dxcPath, dxcDigest, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    const WindowsGameBuildPlan plan =
        Plan(projectText, runtimeBefore, dxcDigest);
    const auto identityRequest = IdentityRequest(iconPath);

    WindowsGameBuildStageResult first;
    auto firstRequest = StageRequest(
        projectRoot,
        outputRoot,
        sourceRuntime,
        dxcPath,
        fixtureRoot,
        "identity-a");
    if (!StageWindowsGameBuild(plan, firstRequest, first, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    WindowsGameExecutableIdentityResult firstIdentity;
    if (!ApplyWindowsGameExecutableIdentity(
            plan,
            identityRequest,
            first,
            firstIdentity,
            error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    const fs::path stamped =
        fs::u8path(first.stagingPath) / plan.executableFileName;
    Digest runtimeAfter;
    if (!Sha256File(sourceRuntime, runtimeAfter, error) ||
        runtimeAfter.bytes != runtimeBefore.bytes ||
        runtimeAfter.sha256 != runtimeBefore.sha256)
    {
        fs::remove_all(root);
        return Fail("Gate 3 modified the source Runtime instead of the staged copy");
    }
    if (firstIdentity.executableSha256.empty() ||
        firstIdentity.executableSha256 == runtimeBefore.sha256 ||
        firstIdentity.applicationManifestPolicy !=
            "asInvoker+PerMonitorV2+longPathAware+utf8")
    {
        fs::remove_all(root);
        return Fail("Gate 3 did not produce a distinct governed executable identity");
    }

    if (VersionString(stamped, L"ProductName", error) != L"Proof Game" ||
        VersionString(stamped, L"CompanyName", error) !=
            L"Maverick Media Studio" ||
        VersionString(stamped, L"ProductVersion", error) !=
            L"0.1.0-gate3" ||
        VersionString(stamped, L"OriginalFilename", error) !=
            L"ProofGame.exe" ||
        VersionString(stamped, L"RenegadeSaveDataId", error) !=
            L"22222222-2222-4222-8222-222222222222" ||
        VersionString(stamped, L"SpecialBuild", error) !=
            L"cc83e5f-gate3-proof" ||
        VersionString(stamped, L"RenegadeBuildTimestampUTC", error) !=
            L"2026-08-10T00:00:00Z")
    {
        fs::remove_all(root);
        return Fail(error.empty()
            ? "Gate 3 VERSIONINFO identity did not round-trip"
            : error);
    }

    const std::string manifest =
        ResourceText(stamped, RT_MANIFEST, 1, error);
    if (manifest.find("level=\"asInvoker\"") == std::string::npos ||
        manifest.find("PerMonitorV2") == std::string::npos ||
        manifest.find("longPathAware") == std::string::npos ||
        manifest.find("UTF-8") == std::string::npos ||
        !HasResource(stamped, RT_GROUP_ICON, 1) ||
        !HasResource(stamped, RT_ICON, 1))
    {
        fs::remove_all(root);
        return Fail(error.empty()
            ? "Gate 3 PE manifest/icon identity is incomplete"
            : error);
    }

    if (first.projectManifestJson.find(
            "\"bootstrap_mode\":\"package_relative\"") ==
            std::string::npos ||
        first.projectManifestJson.find("cc83e5f-gate3-proof") ==
            std::string::npos ||
        first.runtimeSupportManifestJson.find(
            firstIdentity.executableSha256) ==
            std::string::npos ||
        first.packageManifestJson.find(
            firstIdentity.executableSha256) ==
            std::string::npos ||
        !ValidateWindowsGameBuildStage(first, error))
    {
        fs::remove_all(root);
        return Fail(error.empty()
            ? "Gate 3 did not re-manifest the post-identity staged bytes"
            : error);
    }

    if (fs::exists(fs::u8path(first.finalOutputPath)))
    {
        fs::remove_all(root);
        return Fail("Gate 3 created an owner-visible final build");
    }

    WindowsGameBuildStageResult second;
    auto secondRequest = StageRequest(
        projectRoot,
        outputRoot,
        sourceRuntime,
        dxcPath,
        fixtureRoot,
        "identity-b");
    if (!StageWindowsGameBuild(plan, secondRequest, second, error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    WindowsGameExecutableIdentityResult secondIdentity;
    if (!ApplyWindowsGameExecutableIdentity(
            plan,
            identityRequest,
            second,
            secondIdentity,
            error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    if (firstIdentity.executableSha256 != secondIdentity.executableSha256 ||
        first.projectManifestJson != second.projectManifestJson ||
        first.runtimeSupportManifestJson != second.runtimeSupportManifestJson ||
        first.packageManifestJson != second.packageManifestJson ||
        first.projectManifestSha256 != second.projectManifestSha256 ||
        first.runtimeSupportManifestSha256 !=
            second.runtimeSupportManifestSha256 ||
        first.packageManifestSha256 != second.packageManifestSha256)
    {
        fs::remove_all(root);
        return Fail("unchanged Gate 3 identity inputs were not deterministic");
    }

    fs::remove_all(root);
    std::cout << "PASS: LP06 Gate 3 named executable identity and re-manifesting\n";
    return 0;
}
