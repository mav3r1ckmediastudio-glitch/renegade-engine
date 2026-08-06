#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <wiConfig.h>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"

namespace
{
    namespace fs = std::filesystem;

    struct TemporaryDirectory
    {
        fs::path path;

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    int Fail(const fs::path& root, const char* message)
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << "RenegadeIdentityTests: " << message << '\n';
        return 1;
    }

    bool WriteLegacyProject(
        const fs::path& descriptor,
        const std::string& name,
        const std::string& startupScene)
    {
        wi::config::File file;
        file.Open(descriptor.generic_u8string());
        file.Set("format", "renegade-project");
        file.Set("version", 1);
        auto& project = file.GetSection("project");
        project.Set("name", name);
        project.Set("startup_scene", startupScene);
        file.Commit();
        return fs::is_regular_file(descriptor);
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TemporaryDirectory temporary{
        fs::temp_directory_path() /
        fs::u8path("renegade lf01 identity " + std::to_string(unique))
    };
    fs::create_directories(temporary.path);

    const StableId generatedA = GenerateStableId();
    const StableId generatedB = GenerateStableId();
    if (!IsValidStableId(generatedA) ||
        !IsValidStableId(generatedB) ||
        generatedA == generatedB ||
        IsValidStableId("not-a-stable-id"))
    {
        return Fail(temporary.path, "stable ID generation/validation failed");
    }

    const fs::path originalRoot = temporary.path / "Original Project Folder";
    const fs::path scenePath =
        originalRoot / "Content/Scenes/Main.wiscene";
    const fs::path descriptor = originalRoot / "LegacyProject.renegade";
    fs::create_directories(scenePath.parent_path());
    std::ofstream(scenePath, std::ios::binary).put('\0');
    if (!WriteLegacyProject(
            descriptor,
            "Legacy Display Name",
            "Content/Scenes/Main.wiscene"))
    {
        return Fail(temporary.path, "could not write legacy project fixture");
    }

    ProjectService projects;
    projects.Initialize(
        (temporary.path / "Saved/editor-state.ini").generic_u8string());
    if (!projects.OpenProject(descriptor.generic_u8string()))
    {
        return Fail(temporary.path, "legacy project identity was not migrated");
    }
    const StableId projectId = projects.CurrentProject().projectId;
    if (!IsValidStableId(projectId))
    {
        return Fail(temporary.path, "migrated project ID is invalid");
    }

    wi::config::File migratedDescriptor;
    if (!migratedDescriptor.Open(descriptor.generic_u8string()) ||
        migratedDescriptor.GetSection("project").GetText("project_id") != projectId)
    {
        return Fail(temporary.path, "project ID was not persisted to descriptor");
    }

    projects.CloseProject();
    const fs::path movedRoot = temporary.path / "Moved Project Folder";
    fs::rename(originalRoot, movedRoot);
    const fs::path movedDescriptor = movedRoot / descriptor.filename();

    wi::config::File renamedDescriptor;
    renamedDescriptor.Open(movedDescriptor.generic_u8string());
    renamedDescriptor.GetSection("project").Set("name", "Renamed Display Name");
    renamedDescriptor.Commit();

    ProjectMetadata inspected;
    std::string error;
    if (!projects.InspectProject(
            movedDescriptor.generic_u8string(),
            inspected,
            error) ||
        inspected.projectId != projectId ||
        inspected.name != "Renamed Display Name" ||
        fs::u8path(inspected.rootPath) != movedRoot.lexically_normal())
    {
        return Fail(
            temporary.path,
            "project ID did not survive folder/display-name changes");
    }

    DocumentEnvelope envelope = CreateDocumentEnvelope(
        projectId,
        "scene",
        "Content/Scenes/LevelOne.wiscene",
        "lf01-proof");
    const fs::path envelopePath =
        movedRoot / "Content/Data/LevelOne.renegade-doc";
    if (!WriteDocumentEnvelope(
            envelopePath.generic_u8string(),
            envelope,
            error))
    {
        return Fail(temporary.path, "document envelope could not be written");
    }

    DocumentEnvelope loadedEnvelope;
    if (!ReadDocumentEnvelope(
            envelopePath.generic_u8string(),
            loadedEnvelope,
            error) ||
        loadedEnvelope.documentId != envelope.documentId)
    {
        return Fail(temporary.path, "document envelope did not round trip");
    }

    const StableId documentId = loadedEnvelope.documentId;
    if (!RetargetDocumentEnvelope(
            loadedEnvelope,
            "Content/Scenes/Renamed/LevelOne.wiscene",
            error) ||
        loadedEnvelope.documentId != documentId ||
        !WriteDocumentEnvelope(
            envelopePath.generic_u8string(),
            loadedEnvelope,
            error))
    {
        return Fail(
            temporary.path,
            "document rename changed or invalidated its stable ID");
    }

    if (ValidateDocumentEnvelopes(
            projectId,
            std::vector<DocumentEnvelope>{loadedEnvelope, loadedEnvelope},
            error) ||
        error.find("Duplicate document ID") == std::string::npos)
    {
        return Fail(temporary.path, "duplicate document IDs were not rejected");
    }

    DocumentEnvelope malformed = loadedEnvelope;
    malformed.documentId = "broken";
    if (ValidateDocumentEnvelope(malformed, error) ||
        error.find("document ID") == std::string::npos)
    {
        return Fail(temporary.path, "malformed document ID was not rejected");
    }

    SceneService scenes;
    auto& scene = scenes.GetScene();
    const auto entityA = scene.Entity_CreateTransform("Persistent A");
    const auto entityB = scene.Entity_CreateTransform("Persistent B");

    auto validation = ValidatePersistentEntityIdentities(scene);
    if (validation.IsValid() || validation.issues.size() != 2)
    {
        return Fail(temporary.path, "missing entity IDs were not reported");
    }
    if (!EnsurePersistentEntityIdentities(scene, error))
    {
        return Fail(temporary.path, "missing entity IDs were not assigned");
    }

    const StableId entityAId = PersistentEntityId(scene, entityA);
    const StableId entityBId = PersistentEntityId(scene, entityB);
    if (!IsValidStableId(entityAId) ||
        !IsValidStableId(entityBId) ||
        entityAId == entityBId)
    {
        return Fail(temporary.path, "assigned entity IDs are invalid or duplicate");
    }

    EntityIdentityIndex initialIndex;
    if (!initialIndex.Build(scene, error) ||
        initialIndex.Resolve(entityAId) != entityA)
    {
        return Fail(temporary.path, "entity ID lookup index failed");
    }

    DuplicateEntityCommand duplicate(scene, entityA);
    if (!duplicate.Execute())
    {
        return Fail(temporary.path, "entity duplication failed");
    }
    const auto duplicateEntity = duplicate.DuplicatedEntity();
    const StableId duplicateId = PersistentEntityId(scene, duplicateEntity);
    if (!IsValidStableId(duplicateId) || duplicateId == entityAId)
    {
        return Fail(
            temporary.path,
            "normal duplicate did not receive a new persistent ID");
    }
    duplicate.Undo();
    if (!duplicate.Execute() ||
        PersistentEntityId(scene, duplicateEntity) != duplicateId)
    {
        return Fail(
            temporary.path,
            "duplicate Undo/Redo did not preserve its assigned identity");
    }

    auto* metadataB = scene.metadatas.GetComponent(entityB);
    metadataB->string_values.set(PersistentEntityIdMetadataKey, entityAId);
    validation = ValidatePersistentEntityIdentities(scene);
    if (validation.IsValid() ||
        validation.Summary().find("share persistent ID") == std::string::npos)
    {
        return Fail(temporary.path, "duplicate entity IDs were not rejected");
    }
    if (!AssignNewPersistentEntityId(scene, entityB, error))
    {
        return Fail(temporary.path, "duplicate entity ID could not be repaired");
    }

    metadataB = scene.metadatas.GetComponent(entityB);
    metadataB->string_values.set(PersistentEntityIdMetadataKey, "broken");
    validation = ValidatePersistentEntityIdentities(scene);
    if (validation.IsValid() ||
        validation.Summary().find("malformed") == std::string::npos)
    {
        return Fail(temporary.path, "malformed entity ID was not rejected");
    }
    if (!AssignNewPersistentEntityId(scene, entityB, error))
    {
        return Fail(temporary.path, "malformed entity ID could not be repaired");
    }

    SelectionService selection;
    CommandService commands;
    ProjectService noProject;
    noProject.Initialize(
        (temporary.path / "Saved/identity-test-state.ini").generic_u8string());
    SceneDocumentService documents(
        scenes,
        selection,
        commands,
        noProject);
    const fs::path savedScene = temporary.path / "IdentityScene.wiscene";
    if (!documents.Save(savedScene.generic_u8string()))
    {
        return Fail(temporary.path, "scene identity save integration failed");
    }

    SceneService reloaded;
    if (!reloaded.LoadScene(savedScene.generic_u8string()))
    {
        return Fail(temporary.path, "identity scene did not reload");
    }

    EntityIdentityIndex runtimeIndex;
    if (!runtimeIndex.Build(reloaded.GetScene(), error))
    {
        return Fail(temporary.path, "reloaded scene identity index failed");
    }
    const auto remappedEntity = runtimeIndex.Resolve(entityAId);
    if (remappedEntity == wi::ecs::INVALID_ENTITY ||
        remappedEntity == entityA ||
        PersistentEntityId(reloaded.GetScene(), remappedEntity) != entityAId)
    {
        return Fail(
            temporary.path,
            "persistent entity ID did not survive Wicked runtime remapping");
    }

    std::cout
        << "PASS: LF01 stable identity and document envelope contract\n";
    return 0;
}
