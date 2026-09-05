#include "renegade/bridge/BuildIdentityService.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace renegade::bridge::testing
{
    [[nodiscard]] bool DigestWindowsExecutableForRollbackTest(
        const std::string& executablePath,
        std::uint64_t& byteCount,
        std::string& sha256,
        std::string& error);

    [[nodiscard]] bool StampWindowsExecutableWithForcedResourceFailureForTest(
        const std::string& executablePath,
        const WindowsGameBuildPlan& plan,
        const WindowsGameExecutableIdentityRequest& request,
        std::size_t failResourceWriteOrdinal,
        std::size_t& attemptedResourceWrites,
        std::string& error);
}

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
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

    bool WriteIcon(const fs::path& path, std::string& error)
    {
        // Minimal valid 1x1, 32-bit Windows ICO, matching the successful Gate 3
        // identity proof fixture. The forced failure occurs only after this
        // input has passed production preflight and resource update #1 succeeds.
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

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create rollback icon fixture";
            return false;
        }
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
        {
            error = "could not write rollback icon fixture";
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;
    using namespace renegade::bridge::testing;

    if (argc < 1 || argv[0] == nullptr || argv[0][0] == '\0')
        return Fail("rollback test could not identify its source PE");

    const auto nonce = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path(u8"Renegade LP06 Gate3 Rollback Ω " +
            std::to_string(nonce));
    const fs::path stagedExecutable = root / "ProofGame.exe";
    const fs::path iconPath = root / "ProofGame.ico";
    std::string error;
    std::error_code ec;

    fs::create_directories(root, ec);
    if (ec)
        return Fail("could not create rollback fixture directory");

    const fs::path sourceExecutable = fs::absolute(fs::u8path(argv[0]));
    fs::copy_file(
        sourceExecutable,
        stagedExecutable,
        fs::copy_options::overwrite_existing,
        ec);
    if (ec || !WriteIcon(iconPath, error))
    {
        fs::remove_all(root);
        return Fail(ec ? "could not copy rollback PE fixture" : error);
    }

    WindowsGameBuildPlan plan;
    plan.gameName = "Proof Game";
    plan.executableFileName = "ProofGame.exe";
    plan.publicVersion = "0.1.0-gate3";
    plan.saveDataId = "22222222-2222-4222-8222-222222222222";

    WindowsGameExecutableIdentityRequest request;
    request.developerPublisher = "Maverick Media Studio";
    request.description = "Proof Game standalone";
    request.copyrightNotice = "Copyright 2026 Maverick Media Studio";
    request.internalBuildId = "gate3-rollback-proof";
    request.buildTimestampUtc = "2026-08-10T00:00:00Z";
    request.iconSourcePath = iconPath.generic_u8string();

    std::uint64_t beforeBytes = 0;
    std::string beforeSha256;
    if (!DigestWindowsExecutableForRollbackTest(
            stagedExecutable.generic_u8string(),
            beforeBytes,
            beforeSha256,
            error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    std::size_t attemptedWrites = 0;
    const bool unexpectedlySucceeded =
        StampWindowsExecutableWithForcedResourceFailureForTest(
            stagedExecutable.generic_u8string(),
            plan,
            request,
            2,
            attemptedWrites,
            error);
    if (unexpectedlySucceeded || attemptedWrites != 2)
    {
        fs::remove_all(root);
        return Fail(
            "fault injection did not fail after exactly one successful resource write");
    }

    std::uint64_t afterBytes = 0;
    std::string afterSha256;
    if (!DigestWindowsExecutableForRollbackTest(
            stagedExecutable.generic_u8string(),
            afterBytes,
            afterSha256,
            error))
    {
        fs::remove_all(root);
        return Fail(error);
    }

    if (beforeBytes != afterBytes ||
        beforeSha256.empty() ||
        beforeSha256 != afterSha256)
    {
        fs::remove_all(root);
        return Fail(
            "Gate 3 resource transaction did not discard the partial PE update");
    }

    fs::remove_all(root);
    std::cout
        << "PASS: LP06 Gate 3 mid-resource failure leaves staged PE byte-identical\n";
    return 0;
}
