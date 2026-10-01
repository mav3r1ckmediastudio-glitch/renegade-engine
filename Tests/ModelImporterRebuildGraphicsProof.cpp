#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "../WickedEngine/Editor/json.hpp"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <cmath>

#include "../Studio/src/ModelImportPreview.h"
#include "../Runtime/src/RuntimeCharacterCollision.h"
#include "../Runtime/src/RuntimeCharacterAnimation.h"
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
        for (size_t p = 0; p + 4 <= 512 * 320 * 4; p += 4)
        {
            const int difference = std::abs(int(pixels[p]) - int(pixels[0])) +
                std::abs(int(pixels[p + 1]) - int(pixels[1])) +
                std::abs(int(pixels[p + 2]) - int(pixels[2]));
            if (difference > 20) ++visiblePixels;
        }
        std::cout << "MODEL PREVIEW: " << visiblePixels << " contrasting pixels\n";
        return Require(visiblePixels > 100, "thumbnail contains no visible model") &&
            Require(visiblePixels < 512 * 320 * 0.8, "model fills the thumbnail instead of being framed");
    }

    bool VerifyPlacement(const fs::path& root, const StableId& projectId,
        const StableId& assetId)
    {
        auto prepared = CreatorAssetWorkflowService().PrepareModelPlacement(
            root.generic_u8string(), projectId, assetId);
        if (!Require(prepared.IsReady(), "stable-ID placement preparation failed"))
            return false;
        const bool character = IsCharacterAssetTemplateScene(*prepared.PeekScene());
        const auto preparedEvidence = ImportService::SummarizeModelEvidence(*prepared.PeekScene());
        auto scene = wi::allocator::make_shared<wi::scene::Scene>();
        PlaceReusableModelCommand command(*scene, prepared.ReleaseScene(),
            assetId, XMFLOAT3(4.0f, 5.0f, 6.0f), 1.0f, "Placement Proof");
        if (!Require(command.Execute(), "placement command failed")) return false;
        for (size_t i = 0; i < scene->materials.GetCount(); ++i)
            for (const auto& texture : scene->materials[i].textures)
                if (!texture.name.empty() && !Require(texture.resource.IsValid() && texture.resource.GetTexture().IsValid(), "placed model lost a texture")) return false;
        if (character)
        {
            if (!Require(IsRenegadeCharacter(*scene, command.PlacedEntity()),
                "placement did not promote a real Character wrapper")) return false;
            const auto placedEvidence = ImportService::SummarizeModelEvidence(*scene);
            if (!Require(placedEvidence.skinnedMeshes == preparedEvidence.skinnedMeshes &&
                placedEvidence.armatureBones == preparedEvidence.armatureBones &&
                placedEvidence.skinWeightFingerprint == preparedEvidence.skinWeightFingerprint &&
                placedEvidence.inverseBindFingerprint == preparedEvidence.inverseBindFingerprint,
                "placement altered skin weights or inverse bind matrices")) return false;
            for (size_t i = 0; i < scene->animations.GetCount(); ++i)
                if (!Require(!scene->animations[i].IsPlaying(), "Character placement auto-started a clip")) return false;
        }
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
        if (character)
        {
            if (!Require(IsRenegadeCharacter(*reopened, instances.front().instanceRoot),
                "save/reopen lost Character authoring")) return false;
            for (size_t i = 0; i < reopened->animations.GetCount(); ++i)
                if (!Require(!reopened->animations[i].IsPlaying(), "save/reopen auto-started a clip")) return false;
        }
        if (character)
        {
            reopened->Update(0);
            wi::primitive::AABB runtimeBounds;
            if (!Require(ComputeVisibleModelBounds(*reopened, runtimeBounds), "reopened skin has no visible bounds")) return false;
            const auto runtimeCenter = runtimeBounds.getCenter();
            std::cout << "CHARACTER WORLD CENTER: " << runtimeCenter.x << "," << runtimeCenter.y << "," << runtimeCenter.z << "\n";
            // Separate disposable Level for the standalone-player acceptance run.
            TransformState start;
            start.translation = XMFLOAT3(4, 5, 0);
            CreatePlayerStartCommand player(*reopened, start);
            if (!Require(player.Execute(), "Runtime proof Player Start failed")) return false;
            const auto floor = reopened->Entity_CreateCube("Runtime proof floor");
            auto* floorTransform = reopened->transforms.GetComponent(floor);
            floorTransform->Scale(XMFLOAT3(20, 0.5f, 20));
            floorTransform->Translate(XMFLOAT3(4, 4.5f, 6));
            auto& body = reopened->rigidbodies.Create(floor);
            body.mass = 0;
            body.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
            auto& weather = reopened->weathers.Create(wi::ecs::CreateEntity());
            weather.ambient = XMFLOAT3(0.3f, 0.3f, 0.3f);
            const auto light = reopened->Entity_CreateLight("Runtime proof sun");
            reopened->lights.GetComponent(light)->SetType(wi::scene::LightComponent::DIRECTIONAL);
            reopened->transforms.GetComponent(light)->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0));
            const auto runtimePath = (root / "runtime-proof.wiscene").generic_u8string();
            wi::Archive runtimeArchive(runtimePath, false, false);
            reopened->Serialize(runtimeArchive);
            if (!Require(runtimeArchive.SaveFile(runtimePath), "Runtime proof Level save failed")) return false;
            wi::scene::Scene groundedScene;
            wi::Archive groundingCopy;
            reopened->Serialize(groundingCopy);
            groundingCopy.SetReadModeAndResetPos(true);
            groundedScene.Serialize(groundingCopy);
            renegade::runtime::PrepareRuntimeCharacterCollisionScene(groundedScene);
            CharacterRuntimeState runtimeCharacters;
            if (!Require(InitializeRuntimeCharacters(groundedScene, runtimeCharacters, error), error)) return false;
            const auto* initialCharacter = groundedScene.characters.GetComponent(runtimeCharacters.characters.front().entity);
            std::cout << "GROUND START: " << initialCharacter->position.x << "," << initialCharacter->position.y << "," << initialCharacter->position.z << " width=" << initialCharacter->width << " height=" << initialCharacter->height << "\n";
            const auto probe = groundedScene.Intersects(wi::primitive::Ray(XMFLOAT3(4, 10, 6), XMFLOAT3(0, -1, 0)), wi::enums::FILTER_NAVIGATION_MESH);
            std::cout << "FLOOR PROBE: entity=" << probe.entity << " y=" << probe.position.y << "\n";
            for (int frame = 0; frame < 300; ++frame) groundedScene.Update(1.0f / 60.0f);
            const auto* finalCharacter = groundedScene.characters.GetComponent(runtimeCharacters.characters.front().entity);
            std::cout << "GROUND END: " << finalCharacter->position.x << "," << finalCharacter->position.y << "," << finalCharacter->position.z << " grounded=" << finalCharacter->ground_intersect << "\n";
            const auto* grounded = groundedScene.characters.GetComponent(runtimeCharacters.characters.front().entity);
            if (!Require(grounded && grounded->ground_intersect &&
                std::abs(grounded->position.x - 4.0f) < 0.01f &&
                std::abs(grounded->position.y - 5.0f) < 0.01f &&
                std::abs(grounded->position.z - 6.0f) < 0.01f,
                "Runtime Character fell through or was launched off the proof floor")) return false;
            std::cout << "CHARACTER GROUNDED AFTER 5 SECONDS: y=" << grounded->position.y << "\n";
            using namespace renegade::runtime;
            RuntimeCharacterSystemState actors;
            RuntimeCharacterRecord actor;
            actor.entity = instances.front().instanceRoot;
            actor.stableEntityId = PersistentEntityId(*reopened, actor.entity);
            actors.characters.push_back(actor);
            RuntimeCombatState combat;
            CharacterCombatRecord combatRecord;
            combatRecord.characterId = actor.stableEntityId;
            combatRecord.entity = actor.entity;
            combatRecord.health = 100.0f;
            combat.characters.push_back(combatRecord);
            RuntimeCharacterAnimationState animations;
            if (!Require(InitializeRuntimeCharacterAnimations(*reopened, actors, combat, animations, error), error)) return false;
            auto& authored = animations.characters.front();
            for (const auto semantic : {CharacterAnimationSemantic::Idle, CharacterAnimationSemantic::Locomotion,
                CharacterAnimationSemantic::Run, CharacterAnimationSemantic::Attack})
            {
                const auto& variants = authored.clips[CharacterAnimationIndex(semantic)];
                if (variants.empty()) continue;
                if (!Require(RequestCharacterAnimation(*reopened, animations, authored, semantic), "runtime action playback failed") ||
                    !Require(reopened->animations.GetComponent(authored.activeClip)->IsPlaying(), "runtime selected action is paused")) return false;
                std::cout << "RUNTIME AUTHORED ACTION: " << CharacterAnimationSemanticName(semantic) << " / " << variants.size() << " variants\n";
            }
            std::ofstream descriptor(root / "RuntimeProof.renegade");
            descriptor << "format = renegade-project\nversion = 1\n[project]\nproject_id = " << projectId
                << "\nname = Character Runtime Proof\nstartup_scene = runtime-proof.wiscene\n"
                   "startup_flow_id = \nstartup_flow = \nstartup_screen_id = \nstartup_screen = \n"
                   "[dependencies]\nalways_include_format = 1\nalways_include_count = 0\n";
        }
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
    if (argc < 4)
    {
        std::cerr << "Usage: ModelImporterRebuildGraphicsProof <static.glb> <external-uri.glb> <output-root>\n";
        return 2;
    }
    CreatorModelImportRecipe recipeCheck;
    recipeCheck.assetKind = CreatorAssetImportKind::Character;
    CreatorExternalAnimationImportRecipe externalCheck;
    externalCheck.sourceProjectRelativePath = "SourceAssets/Animations/Walk.fbx";
    externalCheck.name = "Walk"; externalCheck.end = 1.0f;
    externalCheck.action = "Walk"; externalCheck.speed = 1.5f; externalCheck.autoMapSource = true;
    recipeCheck.externalAnimations.push_back(externalCheck);
    std::string encoded, recipeError;
    if (!Require(SerializeCreatorModelImportOptions(recipeCheck, encoded, recipeError) &&
        ParseCreatorModelImportOptions(encoded, recipeCheck, recipeError) &&
        recipeCheck.externalAnimations.front().action == "Walk" &&
        recipeCheck.externalAnimations.front().speed == 1.5f &&
        recipeCheck.externalAnimations.front().autoMapSource, "external recipe roundtrip failed")) return 28;
    recipeCheck.externalAnimations.front().action.clear();
    recipeCheck.externalAnimations.front().speed = 1.0f;
    recipeCheck.externalAnimations.front().autoMapSource = false;
    if (!Require(SerializeCreatorModelImportOptions(recipeCheck, encoded, recipeError) &&
        encoded.find("auto_map_source") == std::string::npos &&
        encoded.find("action") == std::string::npos &&
        encoded.find("speed") == std::string::npos &&
        ParseCreatorModelImportOptions(encoded, recipeCheck, recipeError), "legacy external schema changed")) return 28;
    recipeCheck.externalAnimations.front().speed = 0.0f;
    if (!Require(!SerializeCreatorModelImportOptions(recipeCheck, encoded, recipeError), "invalid external speed accepted")) return 28;
    recipeCheck.externalAnimations.front().speed = 1.0f;
    recipeCheck.externalAnimations.front().action = std::string(1, char(10));
    if (!Require(!SerializeCreatorModelImportOptions(recipeCheck, encoded, recipeError), "control-character action accepted")) return 28;
    const bool reopening = std::string(argv[1]) == "--reopen";
    fs::path fixture = fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path externalFixture = fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path root = fs::absolute(fs::u8path(reopening ? argv[2] : argv[3]));
    const bool isFbx = fixture.extension() == ".fbx";
    const fs::path sourceRoot = root.parent_path() / (root.filename().generic_u8string() + "-source");
    if (!reopening && !Require(fs::is_regular_file(fixture) &&
            fs::is_regular_file(externalFixture), "GLB fixtures unavailable")) return 3;

    std::error_code ec;
    if (!reopening) fs::remove_all(root, ec);
    if (!reopening && isFbx)
    {
        fs::remove_all(sourceRoot, ec);
        fs::create_directories(sourceRoot, ec);
        fs::copy(fixture.parent_path(), sourceRoot, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "could not prepare disposable FBX sources")) return 4;
        fixture = sourceRoot / fixture.filename();
    }
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
            if (reopening)
            {
                ReusableModelAssetDocument document;
                std::string error;
                if (!Require(ReadReusableModelAssetDocument((root / "Content/Models/Proof Triangle.rasset").generic_u8string(), document, error), "cold RAsset read failed: " + error)) return 20;
                if (!VerifyPlacement(root, document.manifest.projectId, document.manifest.assetId)) return 21;
                auto retained = ModelImportCandidateService().PrepareModel((root / fs::u8path(argv[3])).generic_u8string());
                if (!Require(retained.IsReady(), "retained FBX cannot reconvert without original source: " + retained.Error())) return 22;
                renegade::studio::ModelImportPreview preview;
                std::vector<std::uint8_t> png;
                if (!Require(preview.Prepare(*retained.PeekMutableScene(), error), error) || !RenderPreview(preview) ||
                    !Require(preview.CapturePng(png, error), error) || !VerifyThumbnailPixels(png)) return 23;
                std::cout << "FBX COLD REOPEN PASS: retained source reconversion and textured placement without original source directory\n";
                return 0;
            }
            auto candidate = ModelImportCandidateService().PrepareModel(
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
                request.characterAsset = candidate.Evidence().skinnedMeshes != 0 &&
                    candidate.Evidence().armatureBones != 0;
                if (fixture.filename() == "animated_character.fbx") {
                    const auto originalEvidence = candidate.Evidence();
                    const auto originalDependencies = candidate.Dependencies().size();
                    std::string rejectedError;
                    const bool appended = ModelImportCandidateService().AppendExternalAnimations(candidate, fixture.generic_u8string(), rejectedError);
                    if (!Require(!appended && !rejectedError.empty() && candidate.Evidence() == originalEvidence &&
                        candidate.Dependencies().size() == originalDependencies, "incomplete humanoid mapping changed candidate")) return 28;
                }
                const auto embeddedCount = candidate.Summary().animations;
                if (request.characterAsset) request.animationActions.assign(embeddedCount, "Unassigned");
                for (int arg = 4; arg < argc; ++arg)
                {
                    const fs::path externalSource = fs::u8path(argv[arg]);
                    const fs::path disposable = sourceRoot / ("external-" + std::to_string(arg)) / externalSource.filename();
                    fs::create_directories(disposable.parent_path());
                    fs::copy_file(externalSource, disposable, fs::copy_options::overwrite_existing);
                    std::string error;
                    const bool appended = ModelImportCandidateService().AppendExternalAnimations(candidate, disposable.generic_u8string(), error);
                    if (!Require(appended, "external animation append failed: " + error)) return 28;
                    request.animationActions.resize(candidate.Summary().animations, arg == 4 ? "Walk" : arg == 5 ? "Run" : arg == 6 ? "Attack" : "Idle");
                }
                const auto unchanged = candidate.Evidence();
                std::string refusalError;
                if (!Require(!ModelImportCandidateService().AppendExternalAnimations(candidate, externalFixture.generic_u8string(), refusalError) &&
                    unchanged == candidate.Evidence(), "invalid external source changed candidate")) return 28;
                renegade::studio::ModelImportPreview preview;
                std::string previewError;
                const auto before = ImportService::Summarize(*candidate.PeekScene());
                const auto evidenceBefore = candidate.Evidence();
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
                if (request.characterAsset)
                {
                    const auto clips = preview.Clips();
                    if (argc > 4 && (!Require(preview.SelectClip(int(embeddedCount)), "external clip selection failed") ||
                        !Require(preview.Scrub((clips[embeddedCount].start + clips[embeddedCount].end) * 0.5f), "external scrub failed") ||
                        !RenderPreview(preview))) return 28;
                    if (argc > 4) {
                        std::vector<std::uint8_t> externalPng;
                        if (!Require(preview.CapturePng(externalPng, previewError), previewError) ||
                            !Require(externalPng != request.thumbnailPng, "external animation did not change rendered pose")) return 28;
                    }
                    if (!Require(!clips.empty(), "Character fixture has no clip")) return 27;
                    std::cout << "CHARACTER EVIDENCE: " << candidate.Evidence().armatureBones
                        << " bones; " << clips.size() << " clips; " << candidate.Dependencies().size() << " textures\n";
                    if (!Require(preview.SelectClip(0) && preview.Scrub((clips[0].start + clips[0].end) * 0.5f),
                        "clip selection/scrub failed") || !RenderPreview(preview)) return 27;
                    std::vector<std::uint8_t> animatedPng;
                    if (!Require(preview.CapturePng(animatedPng, previewError), previewError)) return 27;
                    // The generated moving fixture must visibly evaluate its skinned pose.
                    // An owner reference-pose take need not contain actual movement.
                    if (fixture.filename() == "animated_character.fbx" &&
                        !Require(animatedPng != request.thumbnailPng, "paused scrub did not change the rendered skin pose")) return 27;
                    const float scrubbed = preview.ClipTime();
                    if (!Require(preview.SetSpeed(2.0f) && preview.PlayPause() && preview.IsPlaying(),
                        "preview play or speed failed") || !RenderPreview(preview)) return 27;
                    if (!Require(preview.ClipTime() != scrubbed, "playback time did not advance") ||
                        !Require(preview.PlayPause() && !preview.IsPlaying(), "Pause failed")) return 27;
                    const float pausedTime = preview.ClipTime();
                    if (!RenderPreview(preview) || !Require(preview.ClipTime() == pausedTime,
                        "paused playback time advanced")) return 27;
                    if (clips.size() > 1 && (!Require(preview.SelectClip(1), "second clip selection failed") ||
                        !RenderPreview(preview))) return 27;
                    if (!Require(preview.SelectClip(-1), "reference pose selection failed") || !RenderPreview(preview)) return 27;
                    if (!Require(evidenceBefore == ImportService::SummarizeModelEvidence(*candidate.PeekScene()),
                        "animation preview modified the candidate rig or clips")) return 27;
                    request.assetName = "Static Refusal";
                    const auto staticRefusal = ModelImportCommitService().CommitStaticModel(request, candidate);
                    if (!Require(!staticRefusal.succeeded && !staticRefusal.committed,
                        "static compatibility entry point accepted Character")) return 27;
                    if (!Require(preview.CapturePng(request.thumbnailPng, previewError), previewError)) return 27;
                    std::cout << "CHARACTER PREVIEW SELECT/SCRUB/PLAY/PAUSE/SPEED/ISOLATION PASS\n";
                }
                if (isFbx)
                {
                    if (!Require(!candidate.Dependencies().empty(), "textured FBX has no dependency snapshots")) return 17;
                    const auto& dependency = candidate.Dependencies().front();
                    if (!dependency.sourcePath.empty())
                    {
                        const fs::path texturePath = fs::u8path(dependency.sourcePath);
                        const fs::path hidden = texturePath.generic_u8string() + ".hidden";
                        fs::rename(texturePath, hidden);
                        auto missing = ModelImportCandidateService().PrepareModel(fixture.generic_u8string());
                        request.assetName = "Missing Texture";
                        auto refusedMissing = ModelImportCommitService().CommitModel(request, candidate);
                        fs::rename(hidden, texturePath);
                        if (!Require(!missing.IsReady() && !refusedMissing.committed && !refusedMissing.succeeded &&
                            !fs::exists(root / "Content/Models/Missing Texture.rasset"), "missing texture created a successful candidate or product")) return 18;
                        { std::ofstream changed(texturePath, std::ios::binary | std::ios::app); changed.put('x'); }
                        request.assetName = "Changed Texture";
                        auto refusedChanged = ModelImportCommitService().CommitModel(request, candidate);
                        { std::ofstream restored(texturePath, std::ios::binary | std::ios::trunc); restored.write(reinterpret_cast<const char*>(dependency.bytes.data()), dependency.bytes.size()); }
                        if (!Require(!refusedChanged.committed && !refusedChanged.succeeded && !fs::exists(root / "Content/Models/Changed Texture.rasset"), "changed texture was not rejected before commit")) return 19;
                        std::cout << "FBX MISSING/CHANGED TEXTURE REJECTION PASS\n";
                    }
                }
                if (argc > 4) {
                    const auto& dependency = candidate.Dependencies().back();
                    const fs::path path = fs::u8path(dependency.sourcePath);
                    { std::ofstream changed(path, std::ios::binary | std::ios::app); changed.put('x'); }
                    auto changedRequest = request;
                    changedRequest.assetName = "Changed Animation";
                    const auto refused = ModelImportCommitService().CommitModel(changedRequest, candidate);
                    { std::ofstream restored(path, std::ios::binary | std::ios::trunc); restored.write(reinterpret_cast<const char*>(dependency.bytes.data()), dependency.bytes.size()); }
                    if (!Require(!refused.succeeded && !refused.committed && !fs::exists(root / "Content/Models/Changed Animation.rasset"), "changed animation source committed")) return 28;
                }
                auto invalidThumbnail = request;
                invalidThumbnail.assetName = "Invalid Thumbnail";
                invalidThumbnail.thumbnailPng = {0};
                const auto refusedThumbnail = ModelImportCommitService().CommitModel(
                    invalidThumbnail, candidate);
                if (!Require(!refusedThumbnail.succeeded && !refusedThumbnail.committed &&
                        !fs::exists(root / "Content" / "Models" / "Invalid Thumbnail.rasset"),
                        "invalid thumbnail created a partial model product")) return 14;
                auto external = ModelImportCandidateService().PrepareModel(
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
                    auto externalRequest = request;
                    externalRequest.characterAsset = false;
                    const auto refused = ModelImportCommitService().CommitModel(
                        externalRequest, external);
                    if (!Require(!refused.succeeded && !refused.committed &&
                            !fs::exists(root / "Content" / "Models" /
                                "External Source.rasset"),
                            "external GLB URI was not rejected before project transaction"))
                        resultCode = 8;
                }
                if (resultCode == 0)
                {
                    request.assetName = "Proof Triangle";
                    const auto committed = ModelImportCommitService().CommitModel(
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
                            if (request.characterAsset)
                            {
                                auto placed = CreatorAssetWorkflowService().PrepareModelPlacement(root.generic_u8string(), request.projectId, committed.assetId);
                                if (!Require(placed.IsReady(), "authored Character reopen failed")) return 28;
                                for (size_t i = 0; i < placed.PeekScene()->animations.GetCount(); ++i)
                                {
                                    const auto entity = placed.PeekScene()->animations.GetEntity(i);
                                    const auto* metadata = placed.PeekScene()->metadatas.GetComponent(entity);
                                    if (!Require(metadata && metadata->string_values.get(CreatorCharacterAnimationActionMetadataKey) == request.animationActions[i],
                                        "reopen lost explicit clip action")) return 28;
                                }
                            }
                            const auto duplicate = ModelImportCommitService().CommitModel(
                                request, candidate);
                            if (!Require(!duplicate.succeeded && !duplicate.committed,
                                    "duplicate destination should fail without replacing product"))
                                resultCode = 10;
                            else if (!VerifyPlacement(root, request.projectId, committed.assetId))
                                resultCode = 11;
                            else
                            {
                                if (isFbx)
                                {
                                    for (const auto& d : candidate.Dependencies())
                                    {
                                        const auto retained = root / (d.referenceName.empty() ? "SourceAssets/Animations/Snapshots" : "SourceAssets/Models/Proof Triangle") / fs::u8path(d.retainedRelativePath);
                                        if (!Require(fs::is_regular_file(retained), "retained FBX texture missing")) return 24;
                                        std::ifstream stream(retained, std::ios::binary);
                                        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
                                        if (!Require(bytes == d.bytes, "retained FBX texture bytes changed")) return 24;
                                    }
                                    if (!Require(before == ImportService::Summarize(*candidate.PeekScene()), "commit mutated the import candidate")) return 25;
                                    fs::remove_all(sourceRoot, ec);
                                    if (!Require(!ec && !fs::exists(sourceRoot), "original disposable source directory was not removed")) return 26;
                                    CreatorModelImportRecipe recipe;
                                    const auto settings = nlohmann::json::parse(reopened.manifest.settingsJson);
                                    if (!Require(ParseCreatorModelImportOptions(settings.at("options").dump(), recipe, error), error)) return 28;
                                    auto reimported = ModelImportCandidateService().PrepareModel((root / fs::u8path(committed.sourceProjectRelativePath)).generic_u8string());
                                    if (!Require(reimported.IsReady(), reimported.Error()) ||
                                        !Require(ApplyCreatorModelImportRecipe(*reimported.PeekMutableScene(), root.generic_u8string(), request.projectId, recipe, error), error) ||
                                        !Require(reimported.PeekScene()->animations.GetCount() == candidate.Summary().animations, "retained reimport lost external clips")) return 28;
                                    if (request.characterAsset) for (size_t i = 0; i < reimported.PeekScene()->animations.GetCount(); ++i) {
                                        const auto* metadata = reimported.PeekScene()->metadatas.GetComponent(reimported.PeekScene()->animations.GetEntity(i));
                                        if (!Require(metadata && metadata->string_values.get(CreatorCharacterAnimationActionMetadataKey) == request.animationActions[i], "retained reimport lost action")) return 28;
                                    }
                                    std::cout << "RETAINED REIMPORT CLIPS/ACTIONS PASS\n";
                                }
                                std::cout << "MODEL REBUILD PROOF PASS: placement/Undo/Redo/save/reopen; stable asset="
                                << committed.assetId << "\n";
                            }
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
