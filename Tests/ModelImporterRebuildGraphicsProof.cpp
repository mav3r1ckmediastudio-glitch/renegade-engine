#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <cmath>

#include "../Studio/src/ModelImportPreview.h"
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

    bool RenderPreview(renegade::studio::ModelImportPreview& preview)
    {
        for (int frame = 0; frame < 6000 && !preview.IsReady(); ++frame)
        {
            // Application normally installs newly compiled pipelines here.
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT, 0);
            preview.PreUpdate();
            preview.Update(1.0f / 60.0f);
            preview.PreRender();
            preview.Render();
            wi::graphics::GetDevice()->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
            Sleep(10);
        }
        return Require(preview.IsReady(), "preview never became ready");
    }

    bool VerifyThumbnailPixels(const std::vector<std::uint8_t>& png)
    {
        const auto image = wi::resourcemanager::Load(
            "preview-proof-" + GenerateStableId() + ".png",
            wi::resourcemanager::Flags::NONE, png.data(), png.size());
        if (!Require(image.IsValid() && image.GetTexture().IsValid(),
                "rendered PNG could not decode")) return false;
        const auto& texture = image.GetTexture();
        const auto& desc = texture.GetDesc();
        if (!Require(desc.width == 512 && desc.height == 320 &&
                desc.format == wi::graphics::Format::R8G8B8A8_UNORM,
                "unexpected thumbnail pixel format or size")) return false;
        wi::vector<std::uint8_t> pixels;
        if (!Require(wi::helper::saveTextureToMemory(texture, pixels) &&
                pixels.size() >= 512 * 320 * 4,
                "thumbnail pixels could not be read")) return false;
        size_t visiblePixels = 0;
        for (size_t p = 0; p + 4 <= pixels.size(); p += 4)
        {
            const int difference = std::abs(int(pixels[p]) - int(pixels[0])) +
                std::abs(int(pixels[p + 1]) - int(pixels[1])) +
                std::abs(int(pixels[p + 2]) - int(pixels[2]));
            if (difference > 20) ++visiblePixels;
        }
        std::cout << "MODEL PREVIEW: " << visiblePixels << " contrasting pixels\n";
        return Require(visiblePixels > 100, "thumbnail contains no visible model");
    }

    bool VerifyPlacement(const fs::path& root, const StableId& projectId,
        const StableId& assetId)
    {
        auto prepared = CreatorAssetWorkflowService().PrepareModelPlacement(
            root.generic_u8string(), projectId, assetId);
        if (!Require(prepared.IsReady(), "stable-ID placement preparation failed"))
            return false;
        auto scene = wi::allocator::make_shared<wi::scene::Scene>();
        PlaceReusableModelCommand command(*scene, prepared.ReleaseScene(),
            assetId, XMFLOAT3(4.0f, 5.0f, 6.0f), 1.0f, "Placement Proof");
        if (!Require(command.Execute(), "placement command failed")) return false;
        const auto meshCount = scene->meshes.GetCount();
        const auto objectCount = scene->objects.GetCount();
        const auto wrapperId = PersistentEntityId(*scene, command.PlacedEntity());
        if (!Require(meshCount > 0 && objectCount > 0,
                "placement contains no mesh/object payload")) return false;
        command.Undo();
        std::vector<ReusableAssetInstanceRecord> instances;
        std::string error;
        if (!Require(InspectReusableAssetInstances(*scene, instances, error) &&
                instances.empty() && scene->objects.GetCount() == 0,
                "Undo retained placement payload")) return false;
        if (!Require(command.Execute() &&
                PersistentEntityId(*scene, command.PlacedEntity()) == wrapperId &&
                scene->meshes.GetCount() == meshCount &&
                scene->objects.GetCount() == objectCount,
                "Redo lost placement identity or payload")) return false;
        const auto path = (root / "placement-proof.wiscene").generic_u8string();
        {
            wi::Archive archive(path, false, false);
            if (!Require(archive.IsOpen(), "save archive unavailable")) return false;
            archive.SetCompressionEnabled(true);
            scene->Serialize(archive);
            if (!Require(archive.SaveFile(path), "placement save failed")) return false;
        }
        auto reopened = wi::allocator::make_shared<wi::scene::Scene>();
        {
            wi::Archive archive(path, true);
            if (!Require(archive.IsOpen(), "reopen archive unavailable")) return false;
            reopened->Serialize(archive);
        }
        instances.clear();
        if (!Require(InspectReusableAssetInstances(*reopened, instances, error) &&
                instances.size() == 1 && instances.front().assetId == assetId &&
                PersistentEntityId(*reopened, instances.front().instanceRoot) == wrapperId &&
                reopened->meshes.GetCount() == meshCount &&
                reopened->objects.GetCount() == objectCount,
                "save/reopen lost stable instance identity or payload")) return false;
        const auto* transform = reopened->transforms.GetComponent(
            instances.front().instanceRoot);
        return Require(transform != nullptr &&
            std::abs(transform->translation_local.x - 4.0f) < 0.001f &&
            std::abs(transform->translation_local.y - 5.0f) < 0.001f &&
            std::abs(transform->translation_local.z - 6.0f) < 0.001f,
            "save/reopen lost placement position");
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
        struct PipelineDrain
        {
            ~PipelineDrain()
            {
                while (wi::renderer::IsPipelineCreationActive() > 0) Sleep(10);
                if (wi::graphics::GetDevice()) wi::graphics::GetDevice()->WaitForGPU();
            }
        } pipelineDrain;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device unavailable"))
            resultCode = 6;
        else
        {
            wi::initializer::InitializeComponentsImmediate();
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
                renegade::studio::ModelImportPreview preview;
                std::string previewError;
                const auto before = ImportService::Summarize(*candidate.PeekScene());
                if (!Require(preview.Prepare(*candidate.PeekMutableScene(), previewError),
                        "preview preparation failed: " + previewError)) return 12;
                if (!RenderPreview(preview)) return 12;
                if (!Require(preview.CapturePng(request.thumbnailPng, previewError),
                        "preview capture failed: " + previewError) ||
                    !Require(before == ImportService::Summarize(*candidate.PeekScene()),
                        "preview contaminated the import candidate")) return 12;
                if (!VerifyThumbnailPixels(request.thumbnailPng)) return 15;
                const auto initialThumbnail = request.thumbnailPng;
                preview.Rotate(XM_PI / 4.0f);
                if (!Require(!preview.IsReady(), "rotation retained a stale ready state") ||
                    !RenderPreview(preview) ||
                    !Require(preview.CapturePng(request.thumbnailPng, previewError),
                        "rotated preview capture failed: " + previewError) ||
                    !VerifyThumbnailPixels(request.thumbnailPng) ||
                    !Require(initialThumbnail != request.thumbnailPng,
                        "rotation did not change the rendered thumbnail") ||
                    !Require(before == ImportService::Summarize(*candidate.PeekScene()),
                        "rotation contaminated the import candidate")) return 16;
                auto invalidThumbnail = request;
                invalidThumbnail.assetName = "Invalid Thumbnail";
                invalidThumbnail.thumbnailPng = {0};
                const auto refusedThumbnail = ModelImportCommitService().CommitGlb(
                    invalidThumbnail, candidate);
                if (!Require(!refusedThumbnail.succeeded && !refusedThumbnail.committed &&
                        !fs::exists(root / "Content" / "Models" / "Invalid Thumbnail.rasset"),
                        "invalid thumbnail created a partial model product")) return 14;
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
                        const auto thumbnailPath = root / "Content" / "Models" / "Proof Triangle.thumbnail.png";
                        const auto thumbnail = wi::resourcemanager::Load(thumbnailPath.generic_u8string());
                        if (!Require(thumbnail.IsValid() && thumbnail.GetTexture().IsValid() &&
                                thumbnail.GetTexture().GetDesc().width == 512 &&
                                thumbnail.GetTexture().GetDesc().height == 320,
                                "transaction thumbnail missing or unreadable")) return 13;
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
                            else if (!VerifyPlacement(root, request.projectId, committed.assetId))
                                resultCode = 11;
                            else std::cout << "MODEL REBUILD PROOF PASS: placement/Undo/Redo/save/reopen; stable asset="
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
