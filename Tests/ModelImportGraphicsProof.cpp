#include "renegade/bridge/ImportService.h"

#include <WickedEngine.h>
#include <wiJobSystem.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07ImportProofWindow";

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool ReadBytes(const fs::path& path, std::vector<char>& bytes)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return false;
        }
        input.seekg(0, std::ios::end);
        const auto size = input.tellg();
        if (size < 0)
        {
            return false;
        }
        input.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty())
        {
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
        return input.good() || input.eof();
    }

    bool Require(bool condition, const std::string& message)
    {
        if (condition)
        {
            return true;
        }
        std::cerr << "LP07 FBX PROOF FAIL // " << message << '\n';
        return false;
    }

    std::vector<std::string> CollectAnimationNames(const wi::scene::Scene& scene)
    {
        std::vector<std::string> names;
        names.reserve(scene.animations.GetCount());
        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const auto animationEntity = scene.animations.GetEntity(index);
            const auto* name = scene.names.GetComponent(animationEntity);
            if (name != nullptr && !name->name.empty())
            {
                names.push_back(name->name);
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool LoadSceneForNameProof(
        const fs::path& path,
        wi::scene::Scene& scene,
        std::string& error)
    {
        wi::Archive archive(path.u8string(), true, false);
        if (!archive.IsOpen())
        {
            error = "could not reopen WISCENE for animation-name proof";
            return false;
        }
        scene.Serialize(archive);
        if (archive.GetPos() != archive.GetSize())
        {
            error = "animation-name proof found trailing or incomplete WISCENE data";
            return false;
        }
        return true;
    }

    bool RunImportCase(
        const char* label,
        const fs::path& source,
        const fs::path& destination,
        bool requireSkin,
        bool requireAnimation)
    {
        std::vector<char> sourceBefore;
        if (!Require(ReadBytes(source, sourceBefore),
                std::string(label) + ": could not read source before import"))
        {
            return false;
        }

        renegade::bridge::ImportService importService;
        renegade::bridge::ModelImportRequest request;
        request.sourcePath = source.u8string();
        request.assetPath = destination.u8string();
        request.expectedFormat = renegade::bridge::ModelSourceFormat::Fbx;

        auto prepared = importService.PrepareModelAsset(request);
        if (!Require(prepared.IsReady(),
                std::string(label) + ": prepare failed: " + prepared.Result().error))
        {
            return false;
        }

        const auto preparedResult = prepared.Result();
        if (!Require(
                preparedResult.sourceFormat == renegade::bridge::ModelSourceFormat::Fbx,
                std::string(label) + ": source format was not FBX") ||
            !Require(preparedResult.importerBackend == "wicked.ufbx",
                std::string(label) + ": importer backend was not wicked.ufbx") ||
            !Require(preparedResult.imported.meshes > 0,
                std::string(label) + ": no mesh was imported") ||
            !Require(preparedResult.imported.objects > 0,
                std::string(label) + ": no object was imported") ||
            !Require(preparedResult.sourceBytes == sourceBefore.size(),
                std::string(label) + ": source-byte evidence size mismatch"))
        {
            return false;
        }

        if (!requireSkin &&
            !Require(preparedResult.imported.materials > 0,
                std::string(label) + ": static fixture did not import a material"))
        {
            return false;
        }

        if (requireSkin)
        {
            if (!Require(preparedResult.imported.armatures > 0,
                    std::string(label) + ": no armature was imported") ||
                !Require(preparedResult.importedEvidence.skinnedMeshes > 0,
                    std::string(label) + ": no skinned mesh was imported") ||
                !Require(preparedResult.importedEvidence.primaryInfluenceVertices > 0,
                    std::string(label) + ": no vertex bone weights were imported") ||
                !Require(preparedResult.importedEvidence.armatureBones > 0,
                    std::string(label) + ": armature contains no bones"))
            {
                return false;
            }
        }

        std::vector<std::string> importedAnimationNames;
        if (requireAnimation)
        {
            if (!Require(preparedResult.imported.animations > 0,
                    std::string(label) + ": no animation component was imported") ||
                !Require(preparedResult.importedEvidence.animationChannels > 0,
                    std::string(label) + ": no animation channels were imported") ||
                !Require(preparedResult.importedEvidence.animationSamplers > 0,
                    std::string(label) + ": no animation samplers were imported") ||
                !Require(preparedResult.importedEvidence.animationData > 0,
                    std::string(label) + ": no animation data components were imported") ||
                !Require(preparedResult.importedEvidence.animationKeyframes > 0,
                    std::string(label) + ": no animation keyframe times were imported") ||
                !Require(preparedResult.importedEvidence.animationValues > 0,
                    std::string(label) + ": no animation keyframe values were imported"))
            {
                return false;
            }

            const auto* importedScene = prepared.PeekScene();
            if (!Require(importedScene != nullptr,
                    std::string(label) + ": prepared scene was unavailable for take-name proof"))
            {
                return false;
            }
            importedAnimationNames = CollectAnimationNames(*importedScene);
            if (!Require(!importedAnimationNames.empty(),
                    std::string(label) + ": animated FBX produced no named animation take/clip"))
            {
                return false;
            }
        }

        const auto result = importService.SavePreparedModelAsset(prepared);
        if (!Require(result.succeeded,
                std::string(label) + ": WISCENE save/reopen failed: " + result.error) ||
            !Require(result.imported == result.reloaded,
                std::string(label) + ": structural summary changed after WISCENE reopen") ||
            !Require(result.importedEvidence == result.reloadedEvidence,
                std::string(label) + ": rig/animation evidence changed after WISCENE reopen") ||
            !Require(result.importedEvidence.rigAnimationFingerprint != 0,
                std::string(label) + ": rig/animation fingerprint was zero") ||
            !Require(fs::is_regular_file(destination),
                std::string(label) + ": reusable WISCENE proof output is missing"))
        {
            return false;
        }

        if (requireAnimation)
        {
            wi::scene::Scene reopenedScene;
            std::string reopenError;
            if (!Require(LoadSceneForNameProof(destination, reopenedScene, reopenError),
                    std::string(label) + ": " + reopenError))
            {
                return false;
            }
            const auto reloadedAnimationNames = CollectAnimationNames(reopenedScene);
            if (!Require(reloadedAnimationNames == importedAnimationNames,
                    std::string(label) +
                        ": animation take/clip names changed after WISCENE reopen"))
            {
                return false;
            }
        }

        std::vector<char> sourceAfter;
        if (!Require(ReadBytes(source, sourceAfter),
                std::string(label) + ": could not read source after import") ||
            !Require(sourceAfter == sourceBefore,
                std::string(label) + ": creator FBX bytes changed during import"))
        {
            return false;
        }

        std::cout
            << "LP07 FBX PROOF PASS // " << label
            << " // meshes=" << result.imported.meshes
            << " materials=" << result.imported.materials
            << " armatures=" << result.imported.armatures
            << " animations=" << result.imported.animations
            << " named_animations=" << importedAnimationNames.size()
            << " skinned_meshes=" << result.importedEvidence.skinnedMeshes
            << " bones=" << result.importedEvidence.armatureBones
            << " channels=" << result.importedEvidence.animationChannels
            << " keyframes=" << result.importedEvidence.animationKeyframes
            << " source_bytes=" << result.sourceBytes
            << '\n';
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr
            << "Usage: RenegadeModelImportGraphicsProof "
            << "<static.fbx> <skinned-animated.fbx> <output-directory>\n";
        return 2;
    }

    const fs::path staticFbx = fs::u8path(argv[1]);
    const fs::path skinnedAnimatedFbx = fs::u8path(argv[2]);
    const fs::path outputRoot = fs::u8path(argv[3]);

    if (!Require(fs::is_regular_file(staticFbx), "static FBX fixture is missing") ||
        !Require(fs::is_regular_file(skinnedAnimatedFbx),
            "skinned/animated FBX fixture is missing"))
    {
        return 3;
    }

    std::error_code filesystemError;
    fs::remove_all(outputRoot, filesystemError);
    filesystemError.clear();
    fs::create_directories(outputRoot, filesystemError);
    if (!Require(!filesystemError, "could not create proof output directory"))
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

    const HWND window = CreateWindowExW(
        0,
        WindowClassName,
        L"Renegade LP07 FBX Import Proof",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!Require(window != nullptr, "could not create graphics proof window"))
    {
        return 5;
    }

    // The model converters require a real Wicked graphics device because mesh
    // render data is created during conversion. They do not require unrelated
    // runtime systems such as audio. Initialize the job system explicitly and
    // let Application::SetWindow create the real DX12 device/swapchain without
    // calling Application::Initialize(), which would start all Wicked systems
    // and couples this proof to the hosted runner's XAudio2 capability.
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
            const bool staticPassed = RunImportCase(
                "static-mesh-material",
                staticFbx,
                outputRoot / "static.wiscene",
                false,
                false);
            const bool animatedPassed = RunImportCase(
                "skinned-animated",
                skinnedAnimatedFbx,
                outputRoot / "skinned-animated.wiscene",
                true,
                true);
            if (!staticPassed || !animatedPassed)
            {
                exitCode = 7;
            }
        }
    }

    wi::jobsystem::ShutDown();
    DestroyWindow(window);
    fs::remove_all(outputRoot, filesystemError);

    if (exitCode == 0)
    {
        std::cout << "LP07 FBX GRAPHICS PROOF PASS\n";
    }
    return exitCode;
}
