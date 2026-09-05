#include "renegade/bridge/ReusableAssetService.h"

#include <WickedEngine.h>
#include <wiJobSystem.h>

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07MalformedImportProofWindow";
    constexpr const char* ProjectId = "66666666-6666-4666-8666-666666666666";

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 MALFORMED IMPORT PROOF FAIL // " << message << '\n';
        return false;
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    bool RunMalformedCase(
        const fs::path& projectRoot,
        const std::string& sourceRelative,
        const std::string& assetRelative,
        const renegade::bridge::ModelSourceFormat format,
        const char* label)
    {
        using namespace renegade::bridge;
        ReusableModelImportRequest request;
        request.projectRoot = projectRoot.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = sourceRelative;
        request.assetProjectRelativePath = assetRelative;
        request.expectedFormat = format;

        ReusableAssetService service;
        const auto result = service.ImportModelAsset(request);
        return Require(!result.succeeded,
                std::string(label) + ": malformed source was accepted") &&
            Require(!result.error.empty() && result.error == result.import.error,
                std::string(label) + ": structured ImportService error was not propagated") &&
            Require(!result.transaction.committed,
                std::string(label) + ": malformed source reached the project transaction") &&
            Require(!fs::exists(projectRoot / fs::u8path(assetRelative)),
                std::string(label) + ": malformed source produced an authoritative RAsset");
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: RenegadeReusableAssetMalformedGraphicsProof <output-directory>\n";
        return 2;
    }

    const fs::path outputRoot = fs::u8path(argv[1]);
    const fs::path projectRoot = outputRoot / "Project";
    std::error_code ec;
    fs::remove_all(outputRoot, ec);
    ec.clear();
    fs::create_directories(projectRoot / "Content" / "Models", ec);
    fs::create_directories(projectRoot / "SourceAssets" / "Models", ec);
    fs::create_directories(projectRoot / "Intermediate" / "Transactions", ec);
    if (!Require(!ec, "could not create malformed-import project fixture"))
        return 3;

    if (!Require(WriteText(projectRoot / "SourceAssets/Models/malformed.fbx",
            "this is not an FBX file"), "could not write malformed FBX source") ||
        !Require(WriteText(projectRoot / "SourceAssets/Models/malformed.gltf", "{"),
            "could not write malformed GLTF source"))
    {
        return 4;
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(0, WindowClassName,
        L"Renegade LP07 Malformed Import Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create malformed-import proof window"))
        return 5;

    wi::jobsystem::Initialize();
    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 6;
        }
        else
        {
            const bool fbxPassed = RunMalformedCase(projectRoot,
                "SourceAssets/Models/malformed.fbx",
                "Content/Models/malformed-fbx.rasset",
                renegade::bridge::ModelSourceFormat::Fbx,
                "FBX");
            const bool gltfPassed = fbxPassed && RunMalformedCase(projectRoot,
                "SourceAssets/Models/malformed.gltf",
                "Content/Models/malformed-gltf.rasset",
                renegade::bridge::ModelSourceFormat::Gltf,
                "GLTF");
            if (!fbxPassed || !gltfPassed ||
                !Require(!fs::exists(projectRoot / renegade::bridge::AssetRegistryDocumentName),
                    "malformed imports persisted an asset registry") ||
                !Require(!fs::exists(projectRoot /
                        renegade::bridge::AssetCatalogueMetadataDocumentName),
                    "malformed imports persisted asset metadata"))
            {
                exitCode = 7;
            }
        }
    }

    wi::jobsystem::ShutDown();
    DestroyWindow(window);
    fs::remove_all(outputRoot, ec);
    if (exitCode == 0)
        std::cout << "LP07 MALFORMED IMPORT STRUCTURED ERROR PASS\n";
    return exitCode;
}
