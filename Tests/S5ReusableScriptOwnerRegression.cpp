#include "RuntimeScriptRuntime.h"
#include "RuntimeBootstrap.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int Fail(const std::string& message)
    {
        std::cerr << "S5 reusable script owner regression failed: "
                  << message << '\n';
        return 1;
    }

    bool WriteText(
        const fs::path& path,
        const std::string& text,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create fixture directory: " + ec.message();
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "could not open fixture file";
            return false;
        }
        stream << text;
        if (!stream)
        {
            error = "could not write fixture file";
            return false;
        }
        error.clear();
        return true;
    }

    bool WriteReusableSceneTemplate(
        const fs::path& path,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "could not create template directory: " + ec.message();
            return false;
        }

        wi::scene::Scene scene;
        const wi::ecs::Entity wrapper =
            scene.Entity_CreateTransform("S5 Reusable Barrel");
        const wi::ecs::Entity payload =
            scene.Entity_CreateTransform("Imported Barrel Payload");
        scene.Component_Attach(payload, wrapper, true);

        auto& wrapperMetadata = scene.metadatas.Create(wrapper);
        wrapperMetadata.string_values.set(
            ReusableAssetInstanceIdMetadataKey,
            GenerateStableId());
        auto& payloadMetadata = scene.metadatas.Create(payload);
        payloadMetadata.bool_values.set(
            ReusableAssetPayloadRootMetadataKey,
            true);

        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen())
        {
            error = "could not create template WISCENE";
            return false;
        }
        archive.SetCompressionEnabled(true);
        scene.Serialize(archive);
        const bool saved = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        if (!saved || !fs::is_regular_file(path))
        {
            error = "could not save template WISCENE";
            return false;
        }
        error.clear();
        return true;
    }

    wi::ecs::Entity FindNamedEntity(
        const wi::scene::Scene& scene,
        const std::string& name)
    {
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            if (scene.names[index].name == name)
                return scene.names.GetEntity(index);
        }
        return wi::ecs::INVALID_ENTITY;
    }

    const ScriptAuthoringSource* FindSource(
        const std::vector<ScriptAuthoringSource>& sources,
        const std::string& path)
    {
        for (const auto& source : sources)
        {
            if (source.sourcePath == path)
                return &source;
        }
        return nullptr;
    }

    bool Near(const float left, const float right) noexcept
    {
        return std::fabs(left - right) < 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s5-reusable-owner-" + GenerateStableId());
    const fs::path projectsRoot = root / "Projects";
    const fs::path templateScene = root / "Template.wiscene";
    std::error_code ec;
    fs::create_directories(projectsRoot, ec);
    if (ec)
        return Fail("create projects root: " + ec.message());

    std::string error;
    if (!WriteReusableSceneTemplate(templateScene, error))
        return Fail("write reusable template: " + error);

    StudioSession session;
    session.Projects().Initialize((root / "editor-state.ini").generic_u8string());
    if (!session.Projects().CreateProject(
            projectsRoot.generic_u8string(),
            "S5ReusableOwner",
            templateScene.generic_u8string()))
    {
        return Fail("stage project: " + session.Projects().LastError());
    }

    const std::string scenePath = session.Projects().StartupScenePath();
    if (scenePath.empty() || !session.LoadScene(scenePath))
        return Fail("load project startup Scene");

    const auto& project = session.Projects().CurrentProject();
    DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
        project.projectId,
        "scene",
        project.startupScene,
        "s5-reusable-owner-regression");
    if (!WriteDocumentEnvelope(scenePath + ".rmeta", sceneEnvelope, error))
        return Fail("write Scene identity: " + error);
    if (!session.SaveScene(scenePath))
        return Fail("save Scene to assign persistent identities");

    auto& scene = session.Scenes().GetScene();
    const wi::ecs::Entity wrapper =
        FindNamedEntity(scene, "S5 Reusable Barrel");
    const wi::ecs::Entity payload =
        FindNamedEntity(scene, "Imported Barrel Payload");
    if (wrapper == wi::ecs::INVALID_ENTITY ||
        payload == wi::ecs::INVALID_ENTITY ||
        !scene.Entity_IsDescendant(payload, wrapper))
    {
        return Fail("reusable wrapper/payload hierarchy did not survive load");
    }

    const StableId wrapperId = PersistentEntityId(scene, wrapper);
    const StableId payloadId = PersistentEntityId(scene, payload);
    if (!IsValidStableId(wrapperId) || !IsValidStableId(payloadId) ||
        wrapperId == payloadId)
    {
        return Fail("wrapper/payload persistent identities are invalid");
    }

    const std::string scriptRelative =
        "Content/Scripts/s5_reusable_move.lua";
    const fs::path scriptPath =
        fs::u8path(project.rootPath) / fs::u8path(scriptRelative);
    if (!WriteText(
            scriptPath,
            "if renegade and renegade.metadata then\n"
            " renegade.metadata({schema_version=1,name='S5 Reusable Move',category='Tests',role='SCRIPT',properties={}})\n"
            "end\n"
            "return {\n"
            " on_start=function(self)\n"
            "  assert(self.entity~=nil)\n"
            "  assert(renegade.entity.get_name(self.entity)=='S5 Reusable Barrel')\n"
            "  local ok,err=renegade.transform.translate_local(self.entity,{x=4,y=0,z=0})\n"
            "  assert(ok and not err)\n"
            " end\n"
            "}\n",
            error))
    {
        return Fail("write movement script: " + error);
    }

    std::vector<ScriptAuthoringSource> sources;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    if (!session.Scripts().EnumerateProjectSources(
            ScriptPresentation::Script,
            sources,
            diagnostics,
            error))
    {
        return Fail("enumerate SCRIPT sources: " + error);
    }
    const ScriptAuthoringSource* source = FindSource(sources, scriptRelative);
    if (source == nullptr)
        return Fail("movement script was not discoverable");

    // Reproduce the real editor path: viewport picking can select an imported
    // payload child. Script authoring must promote that child to the durable
    // reusable wrapper before writing the attachment.
    StableId scriptInstanceId;
    if (!session.Scripts().AttachEntitySource(
            payloadId,
            *source,
            scriptInstanceId,
            error))
    {
        return Fail("attach via imported payload child: " + error);
    }

    const ScriptDocument* document = session.Scripts().Document();
    const ScriptAttachment* attachment = document == nullptr
        ? nullptr
        : FindScriptAttachment(*document, scriptInstanceId);
    if (attachment == nullptr || attachment->ownerEntityId != wrapperId)
    {
        return Fail(
            "imported child attachment was not canonicalized to the reusable wrapper");
    }
    if (session.Scripts().EntityAttachments(
            payloadId,
            ScriptPresentation::Script).size() != 1)
    {
        return Fail(
            "Inspector child selection did not resolve wrapper-owned attachment");
    }

    // Simulate reusable payload replacement/reimport. The imported child
    // disappears, while creator-owned wrapper state must remain valid.
    scene.Entity_Remove(payload);
    const wi::ecs::Entity replacement =
        scene.Entity_CreateTransform("Imported Barrel Payload Replacement");
    scene.Component_Attach(replacement, wrapper, true);
    auto& replacementMetadata = scene.metadatas.Create(replacement);
    replacementMetadata.bool_values.set(
        ReusableAssetPayloadRootMetadataKey,
        true);
    if (!AssignNewPersistentEntityId(scene, replacement, error))
        return Fail("assign replacement payload identity: " + error);

    if (!ValidateScriptDocumentAgainstScene(*document, scene, error))
    {
        return Fail(
            "wrapper-owned attachment became invalid after payload replacement: " +
            error);
    }

    TestLevelSnapshotService snapshots(
        session.Scenes(),
        session.Commands(),
        &session.Scripts());
    TestLevelSnapshot snapshot;
    if (!snapshots.Create(project, snapshot, error))
        return Fail("real Test Level snapshot: " + error);

    const auto launch = ParseRuntimeLaunchArguments(
        {"dx12", "--project", snapshot.descriptorPath});
    if (!launch.succeeded)
        return Fail("snapshot launch arguments: " + launch.message);
    const auto resolved = ResolveRuntimeProject(launch);
    if (!resolved.succeeded)
        return Fail("snapshot project resolution: " + resolved.message);

    SceneService runtimeScenes;
    const auto loaded = LoadRuntimeProjectScene(runtimeScenes, resolved);
    if (!loaded.succeeded)
        return Fail("snapshot Runtime Scene load: " + loaded.message);

    RuntimeScriptRuntime runtime;
    if (!runtime.StartSceneFromCompanion(
            resolved.startupScenePath,
            project.projectId,
            runtimeScenes.GetScene(),
            snapshot.sessionDirectory,
            error))
    {
        return Fail("StartSceneFromCompanion: " + error);
    }
    if (runtime.ActiveInstanceCount() != 1 ||
        runtime.DisabledInstanceCount() != 0 ||
        !runtime.Diagnostics().empty())
    {
        return Fail("Runtime did not start one healthy wrapper-owned script");
    }

    EntityIdentityIndex identities;
    if (!identities.Build(runtimeScenes.GetScene(), error))
        return Fail("index Runtime Scene: " + error);
    const wi::ecs::Entity runtimeWrapper = identities.Resolve(wrapperId);
    if (runtimeWrapper == wi::ecs::INVALID_ENTITY)
        return Fail("wrapper identity did not survive Test Level Runtime");
    const auto* transform =
        runtimeScenes.GetScene().transforms.GetComponent(runtimeWrapper);
    if (transform == nullptr || !Near(transform->translation_local.x, 4.0f))
        return Fail("wrapper-owned script did not move the reusable barrel");

    runtime.StopScene();
    if (!snapshots.Cleanup(snapshot, error))
        return Fail("cleanup Test Level snapshot: " + error);

    fs::remove_all(root, ec);
    std::cout
        << "S5 REUSABLE SCRIPT OWNER PASS // imported child selection canonicalizes to stable wrapper and survives payload replacement through Test Level Runtime\n";
    return 0;
}
