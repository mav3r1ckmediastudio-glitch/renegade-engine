#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "../Runtime/src/RuntimeCharacterAnimation.h"
#include "../Runtime/src/RuntimeCombatDecision.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"

#include <WickedEngine.h>
#include <chrono>
#include <cmath>
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07Gate5CreatorAssetProofWindow";
    constexpr const char* ProjectId = "88888888-8888-4888-8888-888888888888";

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

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 5 PROOF FAIL // " << message << '\n';
        return false;
    }

    const renegade::bridge::AssetCatalogueEntry* FindEntry(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(
            catalogue.entries.begin(), catalogue.entries.end(),
            [&id](const auto& entry)
            {
                return entry.registered && entry.assetId == id;
            });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    wi::ecs::Entity FindKeyedNativeAnimation(const wi::scene::Scene& scene)
    {
        for (std::size_t i = 0; i < scene.animations.GetCount(); ++i)
        {
            const auto entity = scene.animations.GetEntity(i);
            const auto& animation = scene.animations[i];
            if (animation.channels.empty() || animation.samplers.empty()) continue;
            for (const auto& sampler : animation.samplers)
            {
                const auto* data = scene.animation_datas.GetComponent(sampler.data);
                if (data != nullptr && !data->keyframe_times.empty() &&
                    !data->keyframe_data.empty()) return entity;
            }
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool PrepareProject(const fs::path& root)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        fs::create_directories(root / "Content" / "Models", ec);
        if (ec) return false;
        fs::create_directories(root / "SourceAssets" / "Models", ec);
        if (ec) return false;
        fs::create_directories(root / "Intermediate" / "Transactions", ec);
        return !ec;
    }

    bool RequireCatalogueQuery(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& assetId,
        const std::string& expectedName)
    {
        using namespace renegade::bridge;

        AssetCatalogueQuery byName;
        byName.text = expectedName;
        const auto nameMatches = QueryAssetCatalogue(catalogue, byName);
        if (!Require(std::any_of(nameMatches.begin(), nameMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
                "catalogue name search did not find the imported FBX product"))
            return false;

        AssetCatalogueQuery byTag;
        byTag.tags = {"hero", "gate5"};
        const auto tagMatches = QueryAssetCatalogue(catalogue, byTag);
        if (!Require(std::any_of(tagMatches.begin(), tagMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
                "catalogue creator-tag search did not find the imported product"))
            return false;

        AssetCatalogueQuery bySystemMetadata;
        bySystemMetadata.sourceFormat = "fbx";
        bySystemMetadata.skinned = true;
        bySystemMetadata.animated = true;
        const auto metadataMatches = QueryAssetCatalogue(catalogue, bySystemMetadata);
        return Require(std::any_of(metadataMatches.begin(), metadataMatches.end(),
                [&assetId](const auto& entry) { return entry.assetId == assetId; }),
            "catalogue FBX/skinned/animated filter did not find the imported product");
    }

    bool ExecutePlacement(
        renegade::bridge::CreatorAssetWorkflowService& workflow,
        const fs::path& projectRoot,
        const renegade::bridge::StableId& assetId,
        renegade::bridge::SceneService& scenes,
        renegade::bridge::CommandService& commands,
        const XMFLOAT3& position,
        wi::ecs::Entity& placedEntity)
    {
        using namespace renegade::bridge;
        auto prepared = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, assetId);
        if (!Require(prepared.IsReady(),
                "registered RAsset could not be prepared for placement: " +
                    prepared.Result().error))
            return false;

        if (!Require(prepared.Result().sceneSummary.meshes > 0 &&
                prepared.Result().sceneSummary.objects > 0,
                "prepared RAsset did not expose placeable model content"))
            return false;

        auto command = std::make_unique<PlaceImportedModelCommand>(
            scenes.GetScene(), prepared.ReleaseScene(), position,
            ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *prepared.PeekScene()));
        // ResolveScaleFactor cannot inspect after ReleaseScene(), so use the
        // accepted automatic result before relinquishing the payload.
        return false;
    }

    bool RunLifecycle(
        const fs::path& projectRoot,
        const fs::path& staticFixture,
        const fs::path& animatedFixture,
        const std::vector<fs::path>& externalSources)
    {
        using namespace renegade::bridge;
        if (!Require(PrepareProject(projectRoot), "project setup failed"))
            return false;

        CreatorAssetWorkflowService workflow;

        // The creator importer already paid the conversion cost to create its
        // visible preview. Prove that exact prepared scene can be retargeted to
        // byte-identical project-owned source bytes, while even a one-byte source
        // change is rejected before governed commit.
        ImportService importer;
        ModelImportRequest previewRequest;
        previewRequest.sourcePath = animatedFixture.generic_u8string();
        previewRequest.assetPath =
            (projectRoot / "Intermediate" / "Imports" / ".retained-preview.wiscene")
                .generic_u8string();
        previewRequest.expectedFormat = ModelSourceFormat::Fbx;
        auto retained = importer.PrepareModelAsset(previewRequest);
        if (!Require(retained.IsReady(),
                "representative FBX could not be prepared once for retained creator commit: " +
                    retained.Result().error))
            return false;

        std::error_code retainedEc;
        const fs::path identicalSource =
            projectRoot / "Intermediate" / "Imports" / "retained-identical.fbx";
        fs::create_directories(identicalSource.parent_path(), retainedEc);
        if (!Require(!retainedEc, "could not create retained-import proof folder"))
            return false;
        fs::copy_file(animatedFixture, identicalSource,
            fs::copy_options::overwrite_existing, retainedEc);
        if (!Require(!retainedEc, "could not copy retained-import proof source"))
            return false;

        ModelImportRequest retargetRequest;
        retargetRequest.sourcePath = identicalSource.generic_u8string();
        retargetRequest.assetPath =
            (projectRoot / "Intermediate" / "Imports" / ".retargeted-preview.wiscene")
                .generic_u8string();
        retargetRequest.expectedFormat = ModelSourceFormat::Fbx;
        std::string retargetError;
        if (!Require(importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "byte-identical retained source was rejected: " + retargetError))
            return false;

        {
            std::ofstream mutate(identicalSource, std::ios::binary | std::ios::app);
            mutate.put('X');
        }
        if (!Require(!importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "mutated retained source was not rejected fail-closed"))
            return false;

        retainedEc.clear();
        fs::copy_file(animatedFixture, identicalSource,
            fs::copy_options::overwrite_existing, retainedEc);
        if (!Require(!retainedEc, "could not restore retained-import proof source") ||
            !Require(importer.RetargetPreparedModelAsset(
                retained, retargetRequest, retargetError),
                "restored byte-identical retained source was rejected: " + retargetError))
            return false;

        PreparedReusableModelPlacement importedPlacement;
        auto imported = workflow.ImportModel(
            projectRoot.generic_u8string(), ProjectId,
            animatedFixture.generic_u8string(),
            "{}", {}, "Content/Models", std::move(retained), {},
            &importedPlacement);
        if (!Require(imported.succeeded && imported.catalogueVerified &&
                imported.reopenedSceneVerified &&
                imported.asset.succeeded &&
                imported.asset.transaction.committed &&
                imported.asset.committedProductVerified,
                "creator FBX import failed: " + imported.error) ||
            !Require(imported.asset.modelMetadata.known &&
                    imported.asset.modelMetadata.skinned &&
                    imported.asset.modelMetadata.animated,
                "representative FBX did not retain skinned/animated metadata"))
            return false;

        // A Model and Character with the same source stem must be distinct
        // governed assets, with independent retained-source namespaces.
        const std::string sharedStem = fs::u8path(imported.assetProjectRelativePath)
            .stem().generic_u8string();
        std::string characterPreflightError;
        if (!Require(workflow.ValidateModelImportDestination(
                projectRoot.generic_u8string(), animatedFixture.generic_u8string(),
                sharedStem, "Content/Characters", characterPreflightError),
                "existing Model blocked same-named Character: " + characterPreflightError))
            return false;
        ModelImportRequest characterRequest;
        characterRequest.sourcePath = animatedFixture.generic_u8string();
        characterRequest.assetPath = (projectRoot / "Intermediate" / "Imports" /
            ".character-preview.wiscene").generic_u8string();
        characterRequest.expectedFormat = ModelSourceFormat::Fbx;
        auto preparedCharacter = importer.PrepareModelAsset(characterRequest);
        if (!Require(preparedCharacter.IsReady(),
                "same-named Character preview preparation failed: " +
                    preparedCharacter.Result().error))
            return false;
        std::size_t originalCharacterClipCount = 0;
        if (!externalSources.empty())
        {
            auto* destinationScene = preparedCharacter.PeekMutableScene();
            if (!Require(destinationScene != nullptr, "prepared Character scene missing"))
                return false;
            std::cout << "MUTANT PRE-MAPPING HUMANOIDS=" << destinationScene->humanoids.GetCount();
            for (std::size_t i = 0; i < destinationScene->humanoids.GetCount(); ++i)
            {
                const auto& h = destinationScene->humanoids[i];
                std::cout << " [lookAt=" << h.IsLookAtEnabled()
                    << " target=" << h.lookAtEntity
                    << " xyz=" << h.lookAt.x << "," << h.lookAt.y << "," << h.lookAt.z << "]";
            }
            std::cout << '\n';
            originalCharacterClipCount = destinationScene->animations.GetCount();
            std::string mappingError;
            if (!Require(EnsureHumanoidAnimationSourceMapping(*destinationScene, mappingError),
                    "external Character destination mapping failed: " + mappingError))
                return false;
            if (!Require(destinationScene->humanoids.GetCount() > 0 &&
                    std::all_of(destinationScene->humanoids.GetComponentArray().begin(),
                                destinationScene->humanoids.GetComponentArray().end(),
                                [](const auto& humanoid) { return !humanoid.IsLookAtEnabled(); }),
                    "external-animation mapping restored unwanted default head look-at"))
                return false;
            wi::ecs::Entity destination = wi::ecs::INVALID_ENTITY;
            for (std::size_t i = 0; i < destinationScene->armatures.GetCount(); ++i)
            {
                const auto rig = destinationScene->armatures.GetEntity(i);
                if (IsHumanoidMappingValid(CaptureHumanoidMapping(*destinationScene, rig)))
                { destination = rig; break; }
            }
            if (!Require(destination != wi::ecs::INVALID_ENTITY,
                    "external Character destination rig missing")) return false;
            for (const auto& externalSource : externalSources)
            {
                const auto retargetStarted = std::chrono::steady_clock::now();
                RetargetHumanoidAnimationsCommand external(*destinationScene, destination,
                    externalSource.generic_u8string(), true);
                if (!Require(external.Execute(),
                        "external animation retarget failed for " +
                            externalSource.filename().generic_u8string() +
                            ": " + external.Result().error)) return false;
                std::cout << "EXTERNAL ANIMATION RETARGET // "
                    << externalSource.filename().generic_u8string() << " // MS=" <<
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now()-retargetStarted).count() << '\n';
            }
        }
        const auto workflowStarted = std::chrono::steady_clock::now();
        auto character = workflow.ImportModel(projectRoot.generic_u8string(), ProjectId,
            animatedFixture.generic_u8string(), "{\"asset_kind\":\"character\"}", sharedStem,
            "Content/Characters", std::move(preparedCharacter), {});
        if (!externalSources.empty())
            std::cout << "EXTERNAL CHARACTER GOVERNED PACKAGE MS=" <<
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now()-workflowStarted).count() << '\n';
        if (!Require(character.succeeded && character.catalogueVerified &&
                    character.reopenedSceneVerified,
                "same-named Character package/import/reopen failed: " + character.error) ||
            !Require(character.stagedSourceProjectRelativePath.find(
                    "SourceAssets/Characters/") == 0 &&
                    character.assetProjectRelativePath.find("Content/Characters/") == 0,
                "Character did not retain source and product in Character namespace") ||
            !Require(fs::is_regular_file(projectRoot /
                    fs::u8path(character.stagedSourceProjectRelativePath)) &&
                    fs::is_regular_file(projectRoot /
                    fs::u8path(character.assetProjectRelativePath)),
                "Character source or product missing after import"))
            return false;
        // A Character import must not disturb the existing Model or either
        // asset's source/product identities in the shared registry.
        AssetRegistry verifiedRegistry;
        std::string registryError;
        if (!Require(ReadAssetRegistry(projectRoot.generic_u8string(), ProjectId,
                verifiedRegistry, registryError),
                "Character import left an unreadable registry: " + registryError))
            return false;
        const auto retainsAsset = [&verifiedRegistry](
            const StableId& id, const std::string& path)
        {
            return std::any_of(verifiedRegistry.records.begin(),
                verifiedRegistry.records.end(), [&](const AssetRecord& record)
                { return record.assetId == id && record.projectRelativePath == path; });
        };
        if (!Require(retainsAsset(imported.asset.assetId,
                    imported.assetProjectRelativePath) &&
                retainsAsset(imported.asset.sourceAssetId,
                    imported.stagedSourceProjectRelativePath) &&
                retainsAsset(character.asset.assetId,
                    character.assetProjectRelativePath) &&
                retainsAsset(character.asset.sourceAssetId,
                    character.stagedSourceProjectRelativePath),
                "Character import discarded or reassigned existing Model/source identity"))
            return false;
        auto reopenedCharacter = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, character.asset.assetId);
        if (!externalSources.empty() &&
            !Require(reopenedCharacter.IsReady() &&
                     reopenedCharacter.PeekScene()->humanoids.GetCount() > 0 &&
                     !reopenedCharacter.PeekScene()->humanoids[0].IsLookAtEnabled(),
                     "governed Character reopened with unwanted head look-at")) return false;
        if (!externalSources.empty() &&
            !Require(reopenedCharacter.IsReady() &&
                     reopenedCharacter.PeekScene()->animations.GetCount() >=
                         originalCharacterClipCount + externalSources.size(),
                     "all external actions were not persisted in reopened Character")) return false;
        if (!externalSources.empty() && reopenedCharacter.IsReady())
        {
            const auto& scene = *reopenedCharacter.PeekScene();
            std::cout << "PERSISTED CHARACTER ANIMATIONS //";
            for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
            {
                const auto entity = scene.animations.GetEntity(index);
                const auto* name = scene.names.GetComponent(entity);
                std::cout << " [" << (name == nullptr ? "<unnamed>" : name->name) << "]";
            }
            std::cout << '\n';
        }
        if (!Require(reopenedCharacter.IsReady() &&
                    FindKeyedNativeAnimation(*reopenedCharacter.PeekScene()) !=
                        wi::ecs::INVALID_ENTITY,
                "same-named Character reopened without native animation keys"))
            return false;
        // Real FBX Character placement must publish the requested WORLD position
        // immediately: Runtime startup reads TransformComponent::GetPosition()
        // before the next Scene::Update and must not reset the NPC to origin.
        {
            SceneService characterScene;
            characterScene.NewScene();
            const XMFLOAT3 requested(17.0f, 3.0f, -9.0f);
            const float scale = ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *reopenedCharacter.PeekScene());
            PlaceImportedModelCommand placeCharacter(
                characterScene.GetScene(), reopenedCharacter.ReleaseScene(),
                requested, scale);
            if (!Require(placeCharacter.Execute(),
                    "real imported Character could not be placed")) return false;
            const auto entity = placeCharacter.PlacedEntity();
            const auto* transform = characterScene.GetScene().transforms.GetComponent(entity);
            if (!Require(transform != nullptr &&
                    std::abs(transform->GetPosition().x - requested.x) < 0.001f &&
                    std::abs(transform->GetPosition().y - requested.y) < 0.001f &&
                    std::abs(transform->GetPosition().z - requested.z) < 0.001f,
                    "real imported Character world transform reset to source origin"))
                return false;
        }

        // Unlike a raw merged FBX, Studio uses a reusable Character wrapper.
        // Verify AI-06 can resolve the clips from THAT logical actor.
        if (!externalSources.empty())
        {
            auto actual = workflow.PrepareModelPlacement(
                projectRoot.generic_u8string(), ProjectId, character.asset.assetId);
            if (!Require(actual.IsReady(), "Character wrapper preparation failed")) return false;
            wi::scene::Scene wrappedScene;
            PlaceReusableModelCommand wrapped(
                wrappedScene, actual.ReleaseScene(), character.asset.assetId,
                XMFLOAT3(17.0f, 3.0f, -9.0f), 1.0f, "Mutant");
            if (!Require(wrapped.Execute(), "real Character wrapper placement failed")) return false;
            if (!Require(IsRenegadeCharacter(wrappedScene, wrapped.PlacedEntity()),
                    "Character folder imported a Model without a real AI Character")) return false;
            // Actual Wicked update overwrites inactive Character transforms too.
            // Prove the real imported instance keeps its pose across a scene tick.
            {
                const auto e = wrapped.PlacedEntity();
                auto* t = wrappedScene.transforms.GetComponent(e);
                auto* c = wrappedScene.characters.GetComponent(e);
                if (!Require(t != nullptr && c != nullptr,
                        "Mutant pose proof missing transform/controller")) return false;
                const auto beforeForward = t->GetForward();
                wrappedScene.dt = 1.0f / 60.0f;
                wi::jobsystem::context characterTick;
                wrappedScene.RunCharacterUpdateSystem(characterTick);
                t = wrappedScene.transforms.GetComponent(e);
                if (!Require(t != nullptr &&
                        std::abs(t->GetPosition().x - 17.0f) < 0.01f &&
                        std::abs(t->GetPosition().y - 3.0f) < 0.01f &&
                        std::abs(t->GetPosition().z + 9.0f) < 0.01f &&
                        std::abs(t->GetForward().x - beforeForward.x) < 0.01f &&
                        std::abs(t->GetForward().z - beforeForward.z) < 0.01f,
                        "real Mutant reset position or flipped after native Wicked Character update")) return false;
            }
            const auto clips = CollectAnimationClips(wrappedScene, wrapped.PlacedEntity(), true);
            std::cout << "REAL MUTANT AI WRAPPER CLIPS=" << clips.size();
            bool walk = false, run = false, attack = false;
            for (const auto& clip : clips)
            {
                std::cout << " [" << clip.name << "]";
                walk |= clip.name.find("mutant walking") != std::string::npos;
                run |= clip.name.find("mutant run") != std::string::npos;
                attack |= clip.name.find("mutant swiping") != std::string::npos;
            }
            std::cout << '\n';
            if (!Require(walk && run && attack,
                    "AI actor wrapper cannot resolve walk/run/swipe clips")) return false;
            using namespace renegade::runtime;
            RuntimeCharacterSystemState actors;
            RuntimeCharacterRecord actor;
            actor.stableEntityId = PersistentEntityId(wrappedScene, wrapped.PlacedEntity());
            actor.entity = wrapped.PlacedEntity();
            actors.characters.push_back(actor);
            RuntimeCombatState combat;
            CharacterCombatRecord weapon;
            weapon.characterId = actor.stableEntityId;
            weapon.entity = actor.entity;
            weapon.health = 100.0f;
            combat.characters.push_back(weapon);
            RuntimeCharacterDecisionState decisions;
            CharacterDecisionRecord decision;
            decision.characterId = actor.stableEntityId;
            decision.intent = CharacterIntent::Idle;
            decisions.characters.push_back(decision);
            RuntimeCharacterAnimationState animations;
            std::string animationError;
            if (!Require(InitializeRuntimeCharacterAnimations(
                    wrappedScene, actors, combat, animations, animationError),
                    "real Mutant AI animation setup failed: " + animationError)) return false;
            auto* selected = FindCharacterAnimation(animations, actor.stableEntityId);
            if (!Require(selected != nullptr, "real Mutant AI actor not indexed")) return false;
            const auto expect = [&](CharacterAnimationSemantic semantic, const char* label)
            {
                const auto* clip = wrappedScene.animations.GetComponent(selected->activeClip);
                return Require(selected->activeSemantic == semantic &&
                    clip != nullptr && clip->IsPlaying() &&
                    selected->resolvedClipName.find(label) != std::string::npos,
                    std::string("real Mutant AI failed native ") + label + " playback");
            };
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Idle, "Mutant")) return false;
            decisions.characters.front().intent = CharacterIntent::Patrol;
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Locomotion, "mutant walking")) return false;
            decisions.characters.front().intent = CharacterIntent::Chase;
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Run, "mutant run")) return false;
            ++combat.characters.front().shotsFired;
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Attack, "mutant swiping")) return false;
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Attack, "mutant swiping")) return false;
            (void)StopAnimation(wrappedScene, selected->activeClip);
            UpdateRuntimeCharacterAnimations(wrappedScene, actors, decisions, combat, animations);
            if (!expect(CharacterAnimationSemantic::Run, "mutant run")) return false;
            std::cout << "REAL MUTANT AI TRANSITIONS // idle -> walk -> run -> swipe -> run PASS\n";
            // Exercise the actual AI-01 -> AI-05 -> AI-06 integration using
            // the real wrapped Mutant and authored intrinsic melee capability.
            auto settings = CaptureCharacterSettings(wrappedScene, actor.entity);
            settings.factionId = "Enemy";
            settings.combatStyle = CombatStyle::Melee;
            SetCharacterSettingsCommand configure(wrappedScene, actor.entity, settings);
            if (!Require(configure.Execute(), "Mutant could not author intrinsic melee")) return false;
            CharacterRuntimeState foundation;
            std::string runtimeError;
            if (!Require(InitializeRuntimeCharacters(wrappedScene, foundation, runtimeError),
                    "real Mutant Runtime foundation failed: " + runtimeError)) return false;
            RuntimeCharacterSystemState liveActors;
            if (!Require(InitializeRuntimeCharacterSystem(
                    wrappedScene, foundation, liveActors, runtimeError),
                    "real Mutant AI profile failed: " + runtimeError)) return false;
            RuntimeCharacterPerceptionState livePerception;
            if (!Require(InitializeRuntimeCharacterPerception(
                    liveActors, livePerception, runtimeError),
                    "real Mutant perception failed: " + runtimeError)) return false;
            RuntimeCharacterDecisionState liveDecisions;
            if (!Require(InitializeRuntimeCharacterDecision(
                    wrappedScene, liveActors, livePerception, liveDecisions, runtimeError),
                    "real Mutant AI decision failed: " + runtimeError)) return false;
            RuntimeCombatState liveCombat;
            if (!Require(InitializeRuntimeCombat(
                    wrappedScene, liveActors, liveCombat, runtimeError),
                    "real Mutant intrinsic combat failed: " + runtimeError)) return false;
            RuntimeCharacterAnimationState liveAnimations;
            if (!Require(InitializeRuntimeCharacterAnimations(
                    wrappedScene, liveActors, liveCombat, liveAnimations, runtimeError),
                    "real Mutant AI06 setup failed: " + runtimeError)) return false;
            auto* liveSelected = FindCharacterAnimation(liveAnimations, actor.stableEntityId);
            if (!Require(liveSelected != nullptr, "real Mutant gameplay clips missing")) return false;
            const auto* native = wrappedScene.characters.GetComponent(actor.entity);
            if (!Require(native != nullptr && native->IsActive(),
                    "real Mutant native controller not active")) return false;
            auto& cognition = livePerception.characters.front();
            cognition.awareness = AwarenessState::Combat;
            cognition.suspicion = 100.0f;
            ++cognition.cognitionTicks;
            CharacterMemoryRecord target;
            target.subjectId = RuntimePlayerKnowledgeId;
            target.subjectFactionId = "Player";
            target.source = KnowledgeSource::Seen;
            target.lastKnownPosition = native->GetPositionInterpolated();
            target.lastKnownPosition.x += 1.0f;
            target.confidence = 1.0f;
            target.threat = 1.0f;
            target.hasPosition = true;
            target.hostile = true;
            target.directSight = true;
            cognition.memories.push_back(target);
            std::uint64_t gameplayEvents = 0;
            const CombatEventEmitter emit = [&gameplayEvents](
                GameplayEvent, std::string& eventError)
            {
                ++gameplayEvents;
                eventError.clear();
                return true;
            };
            UpdateRuntimeCombatDecision(
                wrappedScene, liveActors, livePerception, liveDecisions,
                liveCombat, emit, 1.0f / 60.0f);
            UpdateRuntimeCharacterDecision(
                wrappedScene, liveActors, livePerception, liveDecisions,
                1.0f / 60.0f, false);
            UpdateRuntimeCharacterAnimations(
                wrappedScene, liveActors, liveDecisions, liveCombat,
                liveAnimations);
            if (!Require(liveDecisions.characters.front().intent == CharacterIntent::Attack &&
                    liveCombat.characters.front().shotsFired > 0 &&
                    gameplayEvents > 0 &&
                    liveSelected->activeSemantic == CharacterAnimationSemantic::Attack &&
                    liveSelected->resolvedClipName.find("mutant swiping") != std::string::npos &&
                    wrappedScene.animations.GetComponent(liveSelected->activeClip)->IsPlaying(),
                    "real Mutant AI saw a hostile target but did not attack with native swipe"))
                return false;
            ResetRuntimeCharacters(wrappedScene, foundation);
            std::cout << "REAL MUTANT AUTONOMOUS COMBAT // hostile target -> melee event -> swipe PASS\n";
        }
        if (!Require(importedPlacement.IsReady(),
                "successful creator import did not return an in-memory placement handoff") ||
            !Require(importedPlacement.Result().assetId == imported.asset.assetId &&
                    importedPlacement.Result().sourceAssetId == imported.asset.sourceAssetId &&
                    importedPlacement.Result().assetProjectRelativePath ==
                        imported.assetProjectRelativePath,
                "in-memory placement handoff identity/path differs from committed RAsset") ||
            !Require(importedPlacement.PeekScene() != nullptr &&
                    importedPlacement.Result().sceneSummary.meshes > 0 &&
                    importedPlacement.Result().sceneSummary.objects > 0 &&
                    ImportService::MeasureModelBounds(*importedPlacement.PeekScene()).valid,
                "in-memory placement handoff is not immediately measurable/placeable"))
            return false;

        if (!Require(FindKeyedNativeAnimation(*importedPlacement.PeekScene()) !=
                wi::ecs::INVALID_ENTITY,
                "reopened RAsset placement lost native animation channels or keyed data"))
            return false;

        const StableId sourceId = imported.asset.sourceAssetId;
        const StableId productId = imported.asset.assetId;
        if (!Require(IsValidStableId(sourceId) && IsValidStableId(productId),
                "creator import did not create stable source/product IDs") ||
            !Require(fs::is_regular_file(
                    projectRoot / fs::u8path(imported.stagedSourceProjectRelativePath)),
                "creator import did not retain the project-owned FBX source") ||
            !Require(fs::is_regular_file(
                    projectRoot / fs::u8path(imported.assetProjectRelativePath)),
                "creator import did not create the governed RAsset product"))
            return false;

        std::string error;
        if (!Require(workflow.SetCreatorTags(
                projectRoot.generic_u8string(), ProjectId, productId,
                {"Hero", "gate5"}, error),
                "creator tags could not be persisted: " + error))
            return false;

        AssetCatalogue committedSnapshot;
        if (!Require(workflow.BuildCatalogueSnapshot(
                projectRoot.generic_u8string(), ProjectId, committedSnapshot, error),
                "committed creator catalogue snapshot failed: " + error))
            return false;
        const auto* committedEntry = FindEntry(committedSnapshot, productId);
        if (!Require(committedEntry != nullptr &&
                committedEntry->projectRelativePath ==
                    imported.assetProjectRelativePath &&
                committedEntry->state == AssetCatalogueState::Current &&
                CanPlaceCreatorModelAsset(*committedEntry),
                "committed browser snapshot did not expose the exact stable-ID/path as a placeable model"))
            return false;

        AssetCatalogue catalogue;
        if (!Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "creator catalogue build failed: " + error))
            return false;
        const auto* entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr &&
                entry->state == AssetCatalogueState::Current &&
                entry->sourceFormat == "fbx" &&
                entry->model.skinned && entry->model.animated,
                "creator catalogue did not expose current FBX/system metadata") ||
            !Require(CanPlaceCreatorModelAsset(*entry),
                "creator catalogue entry was not accepted as a placeable model") ||
            !Require(entry->creatorTags == std::vector<std::string>({"gate5", "hero"}),
                "creator tags were not canonicalised/persisted") ||
            !RequireCatalogueQuery(catalogue, productId,
                    fs::u8path(imported.assetProjectRelativePath).stem().generic_u8string()))
            return false;

        const fs::path importedFolder = fs::u8path(
            imported.assetProjectRelativePath).parent_path();
        const AssetBrowserSnapshot browserSnapshot = AssetBrowserService().Scan(
            projectRoot.generic_u8string(), importedFolder.generic_u8string());
        if (!Require(browserSnapshot.succeeded,
                "Asset Browser could not scan the imported product folder: " +
                    browserSnapshot.error) ||
            !Require(std::any_of(
                    browserSnapshot.assets.begin(), browserSnapshot.assets.end(),
                    [&imported](const AssetEntry& asset)
                    {
                        return !asset.directory &&
                            asset.projectRelativePath ==
                                imported.assetProjectRelativePath &&
                            asset.type == AssetType::Model;
                    }),
                "Asset Browser did not expose the newly imported .rasset product"))
            return false;

        // Placement must consume only the registered RAsset. Prove this before
        // the scene lifecycle by temporarily removing the authoritative source.
        const fs::path retainedSource =
            projectRoot / fs::u8path(imported.stagedSourceProjectRelativePath);
        const fs::path temporarilyMissing = retainedSource.string() + ".gate5-missing";
        std::error_code ec;
        fs::rename(retainedSource, temporarilyMissing, ec);
        if (!Require(!ec, "could not temporarily remove retained source"))
            return false;
        auto sourceIndependentPlacement = workflow.PrepareModelPlacement(
            projectRoot.generic_u8string(), ProjectId, productId);
        const bool sourceIndependentReady = sourceIndependentPlacement.IsReady();
        fs::rename(temporarilyMissing, retainedSource, ec);
        if (!Require(!ec, "could not restore retained source") ||
            !Require(sourceIndependentReady,
                "RAsset placement consulted/reconverted the missing FBX source"))
            return false;

        SceneService scenes;
        SelectionService selection;
        CommandService commands;
        ProjectService projects;
        SceneDocumentService documents(scenes, selection, commands, projects);
        scenes.NewScene();

        auto place = [&](const XMFLOAT3& position, wi::ecs::Entity& entity)
        {
            auto prepared = workflow.PrepareModelPlacement(
                projectRoot.generic_u8string(), ProjectId, productId);
            if (!Require(prepared.IsReady(),
                    "repeat RAsset placement preparation failed: " +
                        prepared.Result().error))
                return false;
            const wi::scene::Scene* preparedScene = prepared.PeekScene();
            const float scale = ImportService::ResolveScaleFactor(
                ModelScaleMode::Automatic, *preparedScene);
            auto command = std::make_unique<PlaceImportedModelCommand>(
                scenes.GetScene(), prepared.ReleaseScene(), position, scale);
            auto* raw = command.get();
            if (!Require(commands.Execute(std::move(command)),
                    "PlaceImportedModelCommand failed"))
                return false;
            entity = raw->PlacedEntity();
            return Require(entity != wi::ecs::INVALID_ENTITY,
                "placement command produced no entity");
        };

        wi::ecs::Entity first = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity second = wi::ecs::INVALID_ENTITY;
        if (!place(XMFLOAT3(0.0f, 0.0f, 0.0f), first))
            return false;
        const std::size_t afterFirst = scenes.EntityCount();
        if (!place(XMFLOAT3(2.0f, 0.0f, 0.0f), second))
            return false;
        const std::size_t afterSecond = scenes.EntityCount();
        if (!Require(afterFirst > 0 && afterSecond > afterFirst &&
                commands.UndoCount() == 2 && commands.IsDirty(),
                "two RAsset placements did not enter the normal command stack") ||
            !Require(commands.Undo() && scenes.EntityCount() == afterFirst &&
                    commands.CanRedo(),
                "Undo did not remove the second RAsset placement") ||
            !Require(commands.Redo() && scenes.EntityCount() == afterSecond,
                "Redo did not restore the second RAsset placement"))
            return false;

        const fs::path scenePath = projectRoot / "Content" / "Gate5Placement.wiscene";
        if (!Require(documents.Save(scenePath.generic_u8string()),
                "WISCENE save failed: " + scenes.LastError()) ||
            !Require(!commands.IsDirty(),
                "successful WISCENE save did not mark placement history saved"))
            return false;
        documents.NewScene();
        if (!Require(scenes.EntityCount() == 0,
                "scene close/new did not clear placed entities") ||
            !Require(documents.Open(scenePath.generic_u8string()),
                "saved WISCENE reopen failed: " + scenes.LastError()) ||
            !Require(scenes.EntityCount() == afterSecond,
                "WISCENE reopen did not preserve both RAsset placements"))
            return false;

        const auto reopenedAnimation = FindKeyedNativeAnimation(scenes.GetScene());
        if (!Require(reopenedAnimation != wi::ecs::INVALID_ENTITY &&
                PlayAnimation(scenes.GetScene(), reopenedAnimation, true) &&
                scenes.GetScene().animations.GetComponent(reopenedAnimation)->IsPlaying(),
                "saved/reopened placed FBX lost usable native animation keys or playback"))
            return false;

        // Real source change must be projected as Stale before explicit
        // stable-ID reimport, then return to Current without changing identity.
        fs::copy_file(staticFixture, retainedSource,
            fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "could not update retained FBX source"))
            return false;
        const bool refreshedStaleCatalogue = workflow.BuildCatalogue(
            projectRoot.generic_u8string(), ProjectId, catalogue, error);
        if (!Require(refreshedStaleCatalogue,
                "stale catalogue refresh failed: " + error))
            return false;
        entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr && entry->state == AssetCatalogueState::Stale,
                "changed FBX source was not exposed as Stale"))
            return false;

        const auto reimported = workflow.ReimportModel(
            projectRoot.generic_u8string(), ProjectId, productId);
        if (!Require(reimported.succeeded &&
                reimported.assetId == productId &&
                reimported.sourceAssetId == sourceId,
                "stable-ID explicit reimport failed or changed identity: " +
                    reimported.error) ||
            !Require(workflow.BuildCatalogue(
                projectRoot.generic_u8string(), ProjectId, catalogue, error),
                "post-reimport catalogue refresh failed: " + error))
            return false;
        entry = FindEntry(catalogue, productId);
        if (!Require(entry != nullptr && entry->state == AssetCatalogueState::Current,
                "successful reimport did not return the product to Current") ||
            !Require(entry->creatorTags == std::vector<std::string>({"gate5", "hero"}),
                "creator tags were lost across explicit reimport"))
            return false;

        // Compatibility regression: accepted GLB/GLTF support remains enabled.
        return Require(
            ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Gltf) &&
            ImportService::IsModelSourceFormatSupported(ModelSourceFormat::Glb),
            "GLB/GLTF compatibility contract regressed");
    }
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: RenegadeCreatorAssetWorkflowGraphicsProof "
            << "<static.fbx> <skinned-animated.fbx> <output-directory> [external-animation.fbx ...]\n";
        return 2;
    }

    const fs::path staticFixture = fs::weakly_canonical(fs::u8path(argv[1]));
    const fs::path animatedFixture = fs::weakly_canonical(fs::u8path(argv[2]));
    const fs::path outputRoot = fs::absolute(fs::u8path(argv[3]));
    if (!Require(fs::is_regular_file(staticFixture), "static FBX fixture missing") ||
        !Require(fs::is_regular_file(animatedFixture),
            "skinned/animated FBX fixture missing"))
        return 3;

    const bool assetAuditMode = argc == 6 && std::string(argv[4]) == "--audit-asset";
    const fs::path assetAuditPath = assetAuditMode ? fs::u8path(argv[5]) : fs::path{};
    if (assetAuditMode && !Require(fs::is_regular_file(assetAuditPath), "owner asset audit file missing"))
        return 3;
    const bool auditMode = argc == 6 && std::string(argv[4]) == "--audit-scene";
    const fs::path auditPath = auditMode ? fs::u8path(argv[5]) : fs::path{};
    if (auditMode && !Require(fs::is_regular_file(auditPath), "scene audit file missing"))
        return 3;
    std::vector<fs::path> externalSources;
    for (int index = 4; !auditMode && !assetAuditMode && index < argc; ++index)
    {
        const fs::path source = fs::weakly_canonical(fs::u8path(argv[index]));
        if (!Require(fs::is_regular_file(source),
                "external animation source missing: " + source.generic_u8string()))
            return 3;
        externalSources.push_back(source);
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(0, WindowClassName,
        L"Renegade LP07 Gate 5 Creator Asset Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create Gate 5 graphics proof window"))
        return 4;

    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 5;
        }
        else if (assetAuditMode)
        {
            renegade::bridge::ReusableModelAssetDocument document;
            std::string error;
            if (!renegade::bridge::ReadReusableModelAssetDocument(
                    assetAuditPath.generic_u8string(), document, error))
            {
                std::cerr << "OWNER ASSET AUDIT FAILED: " << error << '\n';
                exitCode = 6;
            }
            else
            {
                fs::create_directories(outputRoot);
                const auto payloadPath = outputRoot / "owner-asset-audit.wiscene";
                std::ofstream payload(payloadPath, std::ios::binary | std::ios::trunc);
                payload.write(reinterpret_cast<const char*>(document.payload.data()),
                    static_cast<std::streamsize>(document.payload.size()));
                payload.close();
                wi::Archive archive(payloadPath.generic_u8string(), true, false);
                wi::scene::Scene actual;
                actual.Serialize(archive);
                std::cout << "OWNER ASSET CLIPS=" << actual.animations.GetCount() << '\n';
                std::size_t usableIdles = 0;
                for (std::size_t i = 0; i < actual.animations.GetCount(); ++i)
                {
                    const auto entity = actual.animations.GetEntity(i);
                    const auto& animation = actual.animations[i];
                    const auto* name = actual.names.GetComponent(entity);
                    const std::string clipName = name ? name->name : "<unnamed>";
                    std::size_t transformTargets = 0;
                    for (const auto& channel : animation.channels)
                        transformTargets += actual.transforms.Contains(channel.target) ? 1 : 0;
                    const bool idle = renegade::runtime::IsExplicitCharacterIdleName(clipName) &&
                        renegade::runtime::InferCharacterAnimationSemantic(clipName) ==
                            renegade::runtime::CharacterAnimationSemantic::Idle;
                    if (idle && transformTargets && animation.end > animation.start + 0.1f)
                        ++usableIdles;
                    std::cout << "OWNER CLIP [" << clipName << "] duration="
                        << animation.end - animation.start << " channels="
                        << animation.channels.size() << " transform_targets="
                        << transformTargets << " explicit_idle=" << idle << '\n';
                }
                std::cout << "OWNER USABLE IDLES=" << usableIdles << '\n';
                // Exercise the actual .rasset's native sampled pose, not just
                // its clip names and channel counts. Nothing writes to the
                // owner's project; the payload is staged under BUILD/recovery.
                wi::ecs::Entity breathing = wi::ecs::INVALID_ENTITY;
                for (std::size_t i = 0; i < actual.animations.GetCount(); ++i)
                {
                    const auto entity = actual.animations.GetEntity(i);
                    const auto* name = actual.names.GetComponent(entity);
                    auto& animation = actual.animations[i];
                    animation.Stop();
                    animation.timer = animation.start;
                    animation.last_update_time = animation.start;
                    if (name && name->name == "mutant breathing idle")
                        breathing = entity;
                }
                if (auto* animation = actual.animations.GetComponent(breathing))
                {
                    std::vector<XMFLOAT4> rotations;
                    std::vector<XMFLOAT3> translations;
                    for (const auto& channel : animation->channels)
                    {
                        const auto* target = actual.transforms.GetComponent(channel.target);
                        rotations.push_back(target ? target->rotation_local : XMFLOAT4{});
                        translations.push_back(target ? target->translation_local : XMFLOAT3{});
                    }
                    animation->SetLooped(true);
                    animation->Play();
                    animation->timer = animation->start + 0.75f;
                    animation->last_update_time = animation->start;
                    actual.dt = 1.0f / 60.0f;
                    actual.ScanAnimationDependencies();
                    wi::jobsystem::context animationContext;
                    actual.RunAnimationUpdateSystem(animationContext);
                    wi::jobsystem::Wait(animationContext);
                    std::size_t movedChannels = 0;
                    for (std::size_t k = 0; k < animation->channels.size(); ++k)
                    {
                        const auto* target = actual.transforms.GetComponent(
                            animation->channels[k].target);
                        if (!target) continue;
                        const auto& r = target->rotation_local;
                        const auto& r0 = rotations[k];
                        const auto& t = target->translation_local;
                        const auto& t0 = translations[k];
                        const float change =
                            std::abs(r.x-r0.x) + std::abs(r.y-r0.y) +
                            std::abs(r.z-r0.z) + std::abs(r.w-r0.w) +
                            std::abs(t.x-t0.x) + std::abs(t.y-t0.y) +
                            std::abs(t.z-t0.z);
                        if (change > 0.0001f) ++movedChannels;
                    }
                    std::cout << "OWNER IDLE NATIVE POSE // moved_channels="
                        << movedChannels << " playing=" << animation->IsPlaying() << '\n';
                    if (!Require(movedChannels > 0,
                            "actual Mutant breathing idle did not animate any skeleton transform"))
                        exitCode = 6;
                }
                if (!Require(usableIdles >= 1,
                        "owner imported asset lacks a targetable, non-bind-pose idle"))
                    exitCode = 6;
            }
        }
        else if (auditMode)
        {
            wi::Archive archive(auditPath.generic_u8string(), true, false);
            if (!Require(archive.IsOpen(), "scene audit could not open archived scene"))
                exitCode = 6;
            else
            {
                wi::scene::Scene auditScene;
                auditScene.Serialize(archive);
                std::cout << "OWNER SCENE AUDIT // native_characters="
                    << auditScene.characters.GetCount() << " authored_characters="
                    << renegade::bridge::CollectCharacters(auditScene).size()
                    << " animation_clips=" << auditScene.animations.GetCount() << '\n';
                for (std::size_t i = 0; i < auditScene.characters.GetCount(); ++i)
                {
                    const auto entity = auditScene.characters.GetEntity(i);
                    const auto* name = auditScene.names.GetComponent(entity);
                    const bool authored = renegade::bridge::IsRenegadeCharacter(auditScene, entity);
                    const auto clips = renegade::bridge::CollectAnimationClips(auditScene, entity, true);
                    std::cout << "OWNER CHARACTER // name=" << (name ? name->name : "<unnamed>")
                        << " authored=" << authored << " clips=" << clips.size();
                    if (authored)
                    {
                        const auto settings = renegade::bridge::CaptureCharacterSettings(auditScene, entity);
                        std::cout << " faction=" << settings.factionId
                            << " combat_style=" << static_cast<int>(settings.combatStyle)
                            << " weapon_id=" << settings.weaponEntityId;
                    }
                    for (const auto& clip : clips) std::cout << " [" << clip.name << "]";
                    std::cout << '\n';
                }
            }
        }
        else if (!RunLifecycle(outputRoot / "creator-asset-project",
                staticFixture, animatedFixture, externalSources))
        {
            exitCode = 6;
        }
    }

    if (IsWindow(window))
        DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);
    if (exitCode == 0)
        std::cout << "LP07 GATE 5 CREATOR ASSET WORKFLOW PROOF PASS\n";
    return exitCode;
}
