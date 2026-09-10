#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptLibraryService.h"
#include "renegade/bridge/ScriptMetadataService.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef RENEGADE_SOURCE_DIR
#error RENEGADE_SOURCE_DIR must be supplied by CMake.
#endif

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int Fail(const std::string& message)
    {
        std::cerr << "Phase 6 objective slice runtime test failed: "
                  << message << '\n';
        return 1;
    }

    bool CopySource(
        const fs::path& source,
        const fs::path& target,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        if (ec)
        {
            error = "create script fixture directory: " + ec.message();
            return false;
        }
        fs::copy_file(
            source,
            target,
            fs::copy_options::overwrite_existing,
            ec);
        if (ec)
        {
            error = "copy script fixture: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    }

    void SetPosition(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3 position)
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        transform->translation_local = position;
        transform->SetDirty();
        transform->UpdateTransform();
    }

    ScriptPropertyValue* FindProperty(
        ScriptAttachment& attachment,
        const std::string& name)
    {
        const auto found = std::find_if(
            attachment.properties.begin(),
            attachment.properties.end(),
            [&](const ScriptPropertyValue& value)
            {
                return value.name == name;
            });
        return found == attachment.properties.end() ? nullptr : &*found;
    }

    void SetBoolean(
        ScriptAttachment& attachment,
        const std::string& name,
        const bool value)
    {
        if (auto* property = FindProperty(attachment, name))
        {
            property->booleanValue = value;
            return;
        }
        ScriptPropertyValue property;
        property.name = name;
        property.type = ScriptPropertyType::Boolean;
        property.booleanValue = value;
        attachment.properties.push_back(std::move(property));
    }

    void SetInteger(
        ScriptAttachment& attachment,
        const std::string& name,
        const std::int64_t value)
    {
        if (auto* property = FindProperty(attachment, name))
        {
            property->integerValue = value;
            return;
        }
        ScriptPropertyValue property;
        property.name = name;
        property.type = ScriptPropertyType::Integer;
        property.integerValue = value;
        attachment.properties.push_back(std::move(property));
    }

    void SetText(
        ScriptAttachment& attachment,
        const std::string& name,
        std::string value)
    {
        if (auto* property = FindProperty(attachment, name))
        {
            property->textValue = std::move(value);
            return;
        }
        ScriptPropertyValue property;
        property.name = name;
        property.type = ScriptPropertyType::String;
        property.textValue = std::move(value);
        attachment.properties.push_back(std::move(property));
    }

    void SetReference(
        ScriptAttachment& attachment,
        const std::string& name,
        StableId value)
    {
        if (auto* property = FindProperty(attachment, name))
        {
            property->referenceId = std::move(value);
            return;
        }
        ScriptPropertyValue property;
        property.name = name;
        property.type = ScriptPropertyType::EntityReference;
        property.referenceId = std::move(value);
        attachment.properties.push_back(std::move(property));
    }

    bool BuildActionAttachment(
        const std::string& projectRoot,
        const std::string& sourcePath,
        const StableId& sourceId,
        const StableId& ownerId,
        ScriptAttachment& attachment,
        std::string& error)
    {
        const auto metadata = EvaluateScriptMetadata(projectRoot, sourcePath);
        if (!metadata.succeeded)
        {
            error = "metadata evaluation failed for " + sourcePath;
            if (!metadata.diagnostics.empty())
                error += ": " + metadata.diagnostics.front().message;
            return false;
        }
        if (metadata.descriptor.presentation != ScriptPresentation::Action)
        {
            error = "objective slice source is not an ACTION: " + sourcePath;
            return false;
        }

        // One governed source path owns one sourceId. Multiple creator
        // instances of the same Action reuse that identity rather than
        // manufacturing a new source identity per attachment.
        ScriptSourceBinding binding;
        binding.sourceId = sourceId;
        binding.sourcePath = sourcePath;
        binding.presentation = ScriptPresentation::Action;
        binding.apiVersion = RuntimeScriptRuntime::ApiVersion;
        binding.provenance.kind = ScriptProvenanceKind::Project;
        binding.provenance.contentHash = "phase6-objective-slice-fixture";

        attachment = CreateScriptAttachment(
            ScriptScope::Entity,
            ownerId,
            binding);
        return ApplyScriptMetadataDefaults(
            metadata.descriptor,
            attachment,
            error);
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    const fs::path sourceRoot = fs::u8path(RENEGADE_SOURCE_DIR);
    const fs::path objectivePackage =
        sourceRoot / "Library" / "Scripts" / "RenegadeObjectiveActions";
    const fs::path stockPackage =
        sourceRoot / "Library" / "Scripts" / "RenegadeStockActions";

    // Prove the reusable objective is a valid Creator Library entry.
    const fs::path libraryProject = fs::temp_directory_path() /
        fs::u8path("renegade-objective-library-" + GenerateStableId());
    std::error_code ec;
    fs::create_directories(libraryProject / "Content" / "Scripts", ec);
    if (ec)
        return Fail("create Creator Library fixture project: " + ec.message());

    ScriptLibraryService library;
    std::vector<ScriptLibraryEntry> entries;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    std::string error;
    if (!library.EnumerateEntries(
            libraryProject.generic_u8string(),
            ScriptPresentation::Action,
            entries,
            diagnostics,
            error,
            (sourceRoot / "Library" / "Scripts").generic_u8string()))
    {
        fs::remove_all(libraryProject, ec);
        return Fail("enumerate reusable objective package: " + error);
    }

    const auto objectiveEntry = std::find_if(
        entries.begin(),
        entries.end(),
        [](const ScriptLibraryEntry& entry)
        {
            return entry.packageId == "renegade.objectives.core" &&
                entry.entryPath == "ObjectiveCounter.lua";
        });
    if (objectiveEntry == entries.end())
    {
        fs::remove_all(libraryProject, ec);
        return Fail("Objective Counter is not discoverable from Creator Library");
    }
    if (!diagnostics.empty())
    {
        const std::string detail = diagnostics.front().message;
        fs::remove_all(libraryProject, ec);
        return Fail("objective package produced a library diagnostic: " + detail);
    }
    fs::remove_all(libraryProject, ec);

    // Execute the actual shipped objective loop with the actual shipped S7
    // Switch, Pickup and Sliding Door Actions.
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-objective-slice-" + GenerateStableId());
    const std::string objectivePath =
        "Content/Scripts/Objectives/ObjectiveCounter.lua";
    const std::string switchPath = "Content/Scripts/Stock/Switch.lua";
    const std::string pickupPath = "Content/Scripts/Stock/Pickup.lua";
    const std::string doorPath = "Content/Scripts/Stock/Door.lua";

    if (!CopySource(
            objectivePackage / "ObjectiveCounter.lua",
            root / fs::u8path(objectivePath),
            error) ||
        !CopySource(
            stockPackage / "Switch.lua",
            root / fs::u8path(switchPath),
            error) ||
        !CopySource(
            stockPackage / "Pickup.lua",
            root / fs::u8path(pickupPath),
            error) ||
        !CopySource(
            stockPackage / "Door.lua",
            root / fs::u8path(doorPath),
            error))
    {
        fs::remove_all(root, ec);
        return Fail(error);
    }

    wi::scene::Scene scene;
    const wi::ecs::Entity playerEntity = scene.Entity_CreateTransform("Player");
    const wi::ecs::Entity switchEntity =
        scene.Entity_CreateTransform("Objective Start Switch");
    const wi::ecs::Entity objectiveEntity =
        scene.Entity_CreateTransform("Objective Controller");
    const wi::ecs::Entity pickup1 = scene.Entity_CreateTransform("Crystal 1");
    const wi::ecs::Entity pickup2 = scene.Entity_CreateTransform("Crystal 2");
    const wi::ecs::Entity pickup3 = scene.Entity_CreateTransform("Crystal 3");
    const wi::ecs::Entity doorEntity = scene.Entity_CreateTransform("Exit Door");

    const StableId playerId = GenerateStableId();
    const StableId switchId = GenerateStableId();
    const StableId objectiveId = GenerateStableId();
    const StableId pickup1Id = GenerateStableId();
    const StableId pickup2Id = GenerateStableId();
    const StableId pickup3Id = GenerateStableId();
    const StableId doorId = GenerateStableId();

    if (!AssignPersistentEntityId(scene, playerEntity, playerId, error) ||
        !AssignPersistentEntityId(scene, switchEntity, switchId, error) ||
        !AssignPersistentEntityId(scene, objectiveEntity, objectiveId, error) ||
        !AssignPersistentEntityId(scene, pickup1, pickup1Id, error) ||
        !AssignPersistentEntityId(scene, pickup2, pickup2Id, error) ||
        !AssignPersistentEntityId(scene, pickup3, pickup3Id, error) ||
        !AssignPersistentEntityId(scene, doorEntity, doorId, error))
    {
        fs::remove_all(root, ec);
        return Fail("assign vertical-slice entity identity: " + error);
    }

    SetPosition(scene, playerEntity, XMFLOAT3(0.0f, 0.0f, 0.0f));
    SetPosition(scene, switchEntity, XMFLOAT3(0.0f, 0.0f, 0.0f));
    SetPosition(scene, objectiveEntity, XMFLOAT3(0.0f, 0.0f, 5.0f));
    SetPosition(scene, pickup1, XMFLOAT3(5.0f, 0.0f, 0.0f));
    SetPosition(scene, pickup2, XMFLOAT3(10.0f, 0.0f, 0.0f));
    SetPosition(scene, pickup3, XMFLOAT3(15.0f, 0.0f, 0.0f));
    SetPosition(scene, doorEntity, XMFLOAT3(20.0f, 0.0f, 0.0f));

    const StableId projectId = GenerateStableId();
    ScriptDocument document = CreateScriptDocument(
        projectId,
        GenerateStableId(),
        "Content/Scenes/ObjectiveSlice.wiscene",
        "phase6-objective-slice-tests");

    // Source identity belongs to the governed source, not an attachment. The
    // three Pickup instances intentionally share pickupSourceId.
    const StableId objectiveSourceId = GenerateStableId();
    const StableId switchSourceId = GenerateStableId();
    const StableId pickupSourceId = GenerateStableId();
    const StableId doorSourceId = GenerateStableId();

    ScriptAttachment objectiveAttachment;
    if (!BuildActionAttachment(
            root.generic_u8string(),
            objectivePath,
            objectiveSourceId,
            objectiveId,
            objectiveAttachment,
            error))
    {
        fs::remove_all(root, ec);
        return Fail(error);
    }
    SetBoolean(objectiveAttachment, "start_active", false);
    SetInteger(objectiveAttachment, "required_count", 3);
    SetReference(objectiveAttachment, "completion_target", doorId);
    SetText(objectiveAttachment, "completion_event", "open");
    SetText(objectiveAttachment, "progress_prefix", "Crystals");
    SetText(objectiveAttachment, "completion_text", "Exit unlocked");
    if (!AddScriptAttachment(document, std::move(objectiveAttachment), error))
    {
        fs::remove_all(root, ec);
        return Fail("attach Objective Counter: " + error);
    }

    ScriptAttachment switchAttachment;
    if (!BuildActionAttachment(
            root.generic_u8string(),
            switchPath,
            switchSourceId,
            switchId,
            switchAttachment,
            error))
    {
        fs::remove_all(root, ec);
        return Fail(error);
    }
    SetReference(switchAttachment, "target", objectiveId);
    SetText(switchAttachment, "event_name", "start_objective");
    SetBoolean(switchAttachment, "one_shot", true);
    SetText(switchAttachment, "prompt_text", "Press E to start objective");
    if (!AddScriptAttachment(document, std::move(switchAttachment), error))
    {
        fs::remove_all(root, ec);
        return Fail("attach start Interaction Switch: " + error);
    }

    const auto addPickup = [&](
        const StableId& pickupId,
        const std::string& prompt) -> bool
    {
        ScriptAttachment pickupAttachment;
        if (!BuildActionAttachment(
                root.generic_u8string(),
                pickupPath,
                pickupSourceId,
                pickupId,
                pickupAttachment,
                error))
        {
            return false;
        }
        SetReference(pickupAttachment, "target", objectiveId);
        SetBoolean(pickupAttachment, "require_interact", true);
        SetText(pickupAttachment, "prompt_text", prompt);
        return AddScriptAttachment(
            document,
            std::move(pickupAttachment),
            error);
    };

    if (!addPickup(pickup1Id, "Press E to collect crystal") ||
        !addPickup(pickup2Id, "Press E to collect crystal") ||
        !addPickup(pickup3Id, "Press E to collect crystal"))
    {
        fs::remove_all(root, ec);
        return Fail("attach objective pickups: " + error);
    }

    ScriptAttachment doorAttachment;
    if (!BuildActionAttachment(
            root.generic_u8string(),
            doorPath,
            doorSourceId,
            doorId,
            doorAttachment,
            error))
    {
        fs::remove_all(root, ec);
        return Fail(error);
    }
    SetBoolean(doorAttachment, "direct_interaction", false);
    if (!AddScriptAttachment(document, std::move(doorAttachment), error))
    {
        fs::remove_all(root, ec);
        return Fail("attach exit Sliding Door: " + error);
    }

    RuntimePlayerState player;
    player.entity = playerEntity;
    GameplayInputFrame input;
    input.interactPressed = true;

    RuntimeScriptRuntime runtime;
    runtime.SetGameplayState(&player, &input);
    if (!runtime.StartScene(
            document,
            scene,
            root.generic_u8string(),
            error))
    {
        fs::remove_all(root, ec);
        return Fail("start objective vertical slice: " + error);
    }
    if (runtime.ActiveInstanceCount() != 6 ||
        runtime.DisabledInstanceCount() != 0 ||
        !runtime.Diagnostics().empty())
    {
        const std::string detail = runtime.Diagnostics().empty()
            ? std::string{}
            : ": " + runtime.Diagnostics().front().message;
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("objective vertical slice did not start cleanly" + detail);
    }

    // Start the objective through a real player Interaction Switch.
    runtime.Update(1.0f / 60.0f);
    if (runtime.CurrentPrompt() != "Press E to start objective" ||
        runtime.PendingEventCount() != 1)
    {
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("Interaction Switch did not start the objective loop");
    }
    input.interactPressed = false;
    runtime.Update(1.0f / 60.0f);

    const auto collect = [&](
        const wi::ecs::Entity pickupEntity,
        const XMFLOAT3 playerPosition,
        const std::string& expectedProgress) -> bool
    {
        SetPosition(scene, playerEntity, playerPosition);
        input.interactPressed = true;
        runtime.Update(1.0f / 60.0f);
        if (runtime.PendingEventCount() == 0)
            return false;

        const auto* pickupTransform =
            scene.transforms.GetComponent(pickupEntity);
        if (pickupTransform == nullptr ||
            pickupTransform->scale_local.x != 0.0f ||
            pickupTransform->scale_local.y != 0.0f ||
            pickupTransform->scale_local.z != 0.0f)
        {
            return false;
        }

        input.interactPressed = false;
        runtime.Update(1.0f / 60.0f);
        return runtime.CurrentPrompt() == expectedProgress;
    };

    if (!collect(pickup1, XMFLOAT3(5.0f, 0.0f, 0.0f), "Crystals 1/3"))
    {
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("first pickup did not advance objective progress to 1/3");
    }
    if (!collect(pickup2, XMFLOAT3(10.0f, 0.0f, 0.0f), "Crystals 2/3"))
    {
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("second pickup did not advance objective progress to 2/3");
    }
    if (!collect(pickup3, XMFLOAT3(15.0f, 0.0f, 0.0f), "Exit unlocked"))
    {
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("third pickup did not complete the objective");
    }

    // Completion emits targeted 'open'. Deliver it and prove the real shipped
    // Sliding Door responds visibly.
    runtime.Update(0.25f);
    const auto* doorTransform = scene.transforms.GetComponent(doorEntity);
    if (doorTransform == nullptr ||
        doorTransform->translation_local.y <= 0.0f)
    {
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("objective completion did not open the target Sliding Door");
    }
    if (runtime.DisabledInstanceCount() != 0 || !runtime.Diagnostics().empty())
    {
        const std::string detail = runtime.Diagnostics().empty()
            ? std::string{}
            : runtime.Diagnostics().front().message;
        runtime.StopScene();
        fs::remove_all(root, ec);
        return Fail("objective vertical slice produced Runtime diagnostics: " + detail);
    }

    runtime.StopScene();
    fs::remove_all(root, ec);
    std::cout << "Phase 6 objective + interaction vertical slice passed.\n";
    return 0;
}
