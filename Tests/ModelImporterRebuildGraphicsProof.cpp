#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <WickedEngine.h>
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace renegade::bridge;

namespace
{
    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
    {
        return DefWindowProcW(window, message, w, l);
    }

    bool Require(bool condition, const std::string& detail)
    {
        if (!condition) std::cerr << "MODEL REBUILD PROOF FAIL: " << detail << '\n';
        return condition;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: ModelImporterRebuildGraphicsProof <static.glb> <external-uri.glb> <output-root>\n";
        return 2;
    }
    const fs::path fixture = fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path externalFixture = fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path root = fs::absolute(fs::u8path(argv[3]));
    if (!Require(fs::is_regular_file(fixture) &&
            fs::is_regular_file(externalFixture), "GLB fixtures unavailable")) return 3;

    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(root / "Content" / "Models", ec);
    if (!Require(!ec, "could not prepare isolated project")) return 4;

    constexpr wchar_t WindowClass[] = L"RenegadeModelImporterRebuildProof";
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = WindowClass;
    RegisterClassExW(&cls);
    const HWND window = CreateWindowExW(0, WindowClass,
        L"Model Import Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, cls.hInstance, nullptr);
    if (!Require(window != nullptr, "graphics proof window unavailable")) return 5;

    int resultCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device unavailable"))
            resultCode = 6;
        else
        {
            auto candidate = ModelImportCandidateService().PrepareGlb(
                fixture.generic_u8string());
            if (!Require(candidate.IsReady(),
                    "isolated GLB conversion failed: " + candidate.Error()) ||
                !Require(candidate.Summary().meshes > 0 &&
                    candidate.Summary().objects > 0,
                    "converted GLB has no mesh/object evidence"))
                resultCode = 7;
            else
            {
                ModelImportCommitRequest request;
                request.projectRoot = root.generic_u8string();
                request.projectId = "88888888-8888-4888-8888-888888888888";
                auto external = ModelImportCandidateService().PrepareGlb(
                    externalFixture.generic_u8string());
                request.assetName = "External Source";
                if (!Require(external.IsReady(),
                        "external URI fixture could not convert for commit rejection proof: " +
                            external.Error()))
                {
                    resultCode = 8;
                }
                else
                {
                    const auto refused = ModelImportCommitService().CommitGlb(
                        request, external);
                    if (!Require(!refused.succeeded && !refused.committed &&
                            !fs::exists(root / "Content" / "Models" /
                                "External Source.rasset"),
                            "external GLB URI was not rejected before project transaction"))
                        resultCode = 8;
                }
                if (resultCode == 0)
                {
                    request.assetName = "Proof Triangle";
                    const auto committed = ModelImportCommitService().CommitGlb(
                        request, candidate);
                    if (!Require(committed.succeeded && committed.committed,
                            "governed commit/reopen failed: " + committed.error))
                        resultCode = 8;
                    else
                    {
                        ReusableModelAssetDocument reopened;
                        std::string error;
                        const fs::path assetPath = root /
                            fs::u8path(committed.assetProjectRelativePath);
                        if (!Require(ReadReusableModelAssetDocument(
                                assetPath.generic_u8string(), reopened, error),
                                "committed RAsset unreadable: " + error) ||
                            !Require(reopened.manifest.assetId == committed.assetId,
                                "reopened asset identity mismatch") ||
                            !Require(fs::is_regular_file(root /
                                fs::u8path(committed.sourceProjectRelativePath)),
                                "retained source is missing"))
                            resultCode = 9;
                        else
                        {
                            const auto duplicate = ModelImportCommitService().CommitGlb(
                                request, candidate);
                            if (!Require(!duplicate.succeeded && !duplicate.committed,
                                    "duplicate destination should fail without replacing product"))
                                resultCode = 10;
                            else std::cout << "MODEL REBUILD PROOF PASS: stable asset="
                                << committed.assetId << "\n";
                        }
                    }
                }
            }
        }
    }
    DestroyWindow(window);
    UnregisterClassW(WindowClass, cls.hInstance);
    return resultCode;
}
