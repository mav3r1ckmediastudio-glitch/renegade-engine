#include "renegade/bridge/TerrainPropPage.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

using namespace renegade::bridge;
namespace fs = std::filesystem;

static void Check(bool pass, const char* message)
{
    if (!pass) throw std::runtime_error(message);
}

static std::vector<std::uint8_t> ReadBytes(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    Check(file.is_open(), "missing committed file");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main()
{
    const auto root = fs::temp_directory_path() / ("renegade-prop-page-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        Check(fs::create_directory(root), "test directory already exists");
        TerrainPropPage page;
        page.cellX = -8;
        page.profiles = {{{1}}, {{2}}, {{3}}};
        page.placements = {{0x100000001ull, 0, {1,2,3}, 1, 2, 71},
            {22, 1, {4,5,6}, 2, 1}, {33, 2, {7,8,9}, 0, 1}};
        std::vector<std::uint8_t> bytes;
        std::string error;
        Check(EncodeTerrainPropPage(page, bytes, error), "encode");
        Check(bytes.size() == 192, "wire layout");
        ProjectDocumentTransactionOptions options;
        options.allowedRoot = root.string();
        options.journalDirectory = (root / "journal").string();
        const auto pagePath = root / "page.bin";
        const auto pairedPath = root / "revision";
        std::vector<ProjectDocumentWrite> writes{
            {pagePath.string(), bytes, {}}, {pairedPath.string(), {1}, {}}};
        ProjectDocumentTransaction transaction;
        Check(transaction.Execute(writes, options).success, "first commit");
        TerrainPropPage reopened;
        Check(DecodeTerrainPropPage(ReadBytes(pagePath), reopened, error), "reopen");
        std::vector<std::uint8_t> again;
        Check(EncodeTerrainPropPage(reopened, again, error) && again == bytes, "identity and transform round trip");
        Check(reopened.placements[0].candidateKey == 71, "candidate key round trip");
        auto legacy = bytes;
        for (int n = 2; n >= 0; --n) legacy.erase(legacy.begin() + 24 + 48 + n * 40 + 32, legacy.begin() + 24 + 48 + n * 40 + 40);
        legacy[4] = 1;
        TerrainPropPage migrated;
        Check(DecodeTerrainPropPage(legacy, migrated, error) && migrated.placements[0].candidateKey == 0 && migrated.placements[0].id == page.placements[0].id, "v1 read compatibility");
        auto damaged = bytes;
        damaged[4] = 3;
        Check(!DecodeTerrainPropPage(damaged, reopened, error), "future version");
        damaged = bytes;
        damaged.pop_back();
        Check(!DecodeTerrainPropPage(damaged, reopened, error), "truncation");
        Check(reopened.placements.size() == 3, "unchanged after failed decode");
        auto duplicate = page;
        duplicate.placements.back().id = duplicate.placements.front().id;
        Check(!EncodeTerrainPropPage(duplicate, again, error) && again == bytes, "duplicate ID");
        page.placements[0].scale = std::numeric_limits<float>::quiet_NaN();
        Check(!EncodeTerrainPropPage(page, again, error), "nonfinite scale");
        page.placements[0].scale = 3;
        Check(EncodeTerrainPropPage(page, writes[0].content, error), "second page");
        writes[1].content = {2};
        options.operationHook = [](auto stage, std::size_t index, const auto&, auto&)
        {
            return stage == ProjectDocumentTransactionStage::AfterReplace && index == 0
                ? ProjectDocumentTransactionHookAction::Fail : ProjectDocumentTransactionHookAction::Continue;
        };
        auto failed = transaction.Execute(writes, options);
        Check(!failed.success && failed.rolledBack, "forced rollback");
        Check(ReadBytes(pagePath) == bytes && ReadBytes(pairedPath) == std::vector<std::uint8_t>{1}, "previous revision");
        options.operationHook = [](auto stage, std::size_t index, const auto&, auto&)
        {
            return stage == ProjectDocumentTransactionStage::AfterReplace && index == 0
                ? ProjectDocumentTransactionHookAction::Interrupt : ProjectDocumentTransactionHookAction::Continue;
        };
        const auto interrupted = transaction.Execute(writes, options);
        Check(interrupted.recoveryRequired && !interrupted.success, "interrupt did not leave recovery journal");
        options.operationHook = {};
        const auto recovery = transaction.Recover(interrupted.journalPath, options);
        Check(recovery.success && recovery.rolledBack && ReadBytes(pagePath) == bytes &&
            ReadBytes(pairedPath) == std::vector<std::uint8_t>{1}, "recovered revision mismatch");
        Check(transaction.Execute(writes, options).success, "retry");
        Check(DecodeTerrainPropPage(ReadBytes(pagePath), reopened, error) &&
            reopened.placements[0].id == 0x100000001ull && reopened.placements[0].scale == 3,
            "same identity after retry");
        fs::remove_all(root);
        std::cout << "PASS page v1 migration and v2 round trip, corrupt inputs, stable IDs, rollback and retry\n";
        return 0;
    }
    catch (const std::exception& failure)
    {
        std::cerr << failure.what() << " evidence=" << root << '\n';
        return 1;
    }
}
