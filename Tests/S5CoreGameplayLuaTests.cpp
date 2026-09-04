#include "RuntimeScriptRuntime.h"
#include "RuntimeBootstrap.h"

#include "renegade/bridge/IdentityService.h"
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
        std::cerr << "S5 core gameplay Lua test failed: " << message << '\n';
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
            error = "could not open fixture file: " + path.generic_u8string();
            return false;
        }
        stream << text;
        if (!stream)
        {
            error = "could not write fixture file: " + path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    bool WriteSceneTemplate(
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
        scene.Entity_CreateTransform("S5 Test Barrel");
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
        fs::u8path("renegade-s5a-lua-" + GenerateStableId());
    const fs::path scriptPath = root / "Content" / "Scripts" / "move_barrel.lua";

    std::string error;
    if (!WriteText(
            scriptPath,
            "return {\n"
            " on_start=function(self)\n"
            "  assert(self.entity~=nil)\n"
            "  assert(renegade.entity.get_name(self.entity)=='S5 Test Barrel')\n"
            "  local p,err=renegade.transform.get_local_position(self.entity)\n"
            "  assert(p and not err and p.x==0 and p.y==0 and p.z==0)\n"
            "  local ok,seterr=renegade.transform.set_local_position(self.entity,{x=1,y=2,z=3})\n"
            "  assert(ok and not seterr)\n"
            "  ok,seterr=renegade.transform.translate_local(self.entity,{x=0.5,y=-1,z=2})\n"
            "  assert(ok and not seterr)\n"
            "  local bad,baderr=renegade.transform.set_local_position(self.entity,{x='bad',y=0,z=0})\n"
            "  assert(bad==false and type(baderr)=='string')\n"
            " end\n"
            "}\n",
            error))
    {
        return Fail("write fixture: " + error);
    }

    wi::scene::Scene scene;
    const auto barrel = scene.Entity_CreateTransform("S5 Test Barrel");
    const StableId barrelId = GenerateStableId();
    if (!AssignPersistentEntityId(scene, barrel, barrelId, error))
        return Fail("assign persistent barrel ID: " + error);

    const StableId projectId = GenerateStableId();
    const StableId sceneDocumentId = GenerateStableId();
    ScriptDocument document = CreateScriptDocument(
        projectId,
        sceneDocumentId,
        "Content/Scenes/S5A.wiscene",
        "s5a-tests");

    ScriptSourceBinding source;
    source.sourceId = GenerateStableId();
    source.sourcePath = "Content/Scripts/move_barrel.lua";
    source.presentation = ScriptPresentation::Script;
    source.apiVersion = RuntimeScriptRuntime::ApiVersion;
    source.provenance.kind = ScriptProvenanceKind::Project;
    source.provenance.contentHash = "s5a-fixture";

    if (!AddScriptAttachment(
            document,
            CreateScriptAttachment(ScriptScope::Entity, barrelId, source),
            error))
    {
        return Fail("attach fixture script: " + error);
    }

    RuntimeScriptRuntime runtime;
    if (!runtime.StartScene(document, scene, root.generic_u8string(), error))
        return Fail("StartScene: " + error);
    if (runtime.DisabledInstanceCount() != 0 || !runtime.Diagnostics().empty())
        return Fail("valid S5A API calls disabled the script instance");

    const auto* transform = scene.transforms.GetComponent(barrel);
    if (transform == nullptr)
        return Fail("barrel transform disappeared");
    const XMFLOAT3 local = transform->translation_local;
    if (!Near(local.x, 1.5f) || !Near(local.y, 1.0f) || !Near(local.z, 5.0f))
        return Fail("governed Lua did not update the barrel local transform");
    const XMFLOAT3 world = transform->GetPosition();
    if (!Near(world.x, 1.5f) || !Near(world.y, 1.0f) || !Near(world.z, 5.0f))
        return Fail("governed Lua did not move the barrel visibly through the Wicked world transform");

    runtime.StopScene();

    // Real owner-workflow regression. This deliberately goes through the
    // Studio authoring service, LP04 Test Level snapshot, Runtime project
    // bootstrap and StartSceneFromCompanion. The attachment remains unsaved so
    // a green result proves Test Level consumes the live scripting document,
    // rather than accidentally reading an older .rscripts file from disk.
    const fs::path projectsRoot = root / "Projects";
    const fs::path templateScene = root / "Template.wiscene";
    std::error_code ec;
    fs::create_directories(projectsRoot, ec);
    if (ec)
        return Fail("create snapshot fixture projects root: " + ec.message());
    if (!WriteSceneTemplate(templateScene, error))
        return Fail("write snapshot fixture template: " + error);

    StudioSession session;
    session.Projects().Initialize((root / "editor-state.ini").generic_u8string());
    if (!session.Projects().CreateProject(
            projectsRoot.generic_u8string(),
            "S5ASnapshot",
            templateScene.generic_u8string()))
    {
        return Fail(
            "stage snapshot fixture project: " +
            session.Projects().LastError());
    }

    const std::string authoredScenePath = session.Projects().StartupScenePath();
    if (authoredScenePath.empty() || !session.LoadScene(authoredScenePath))
        return Fail("adopt snapshot fixture project and startup Scene");
    if (!session.Projects().HasProject())
        return Fail("snapshot fixture project did not become authoritative");

    const auto& authoredProject = session.Projects().CurrentProject();
    DocumentEnvelope authoredSceneEnvelope = CreateDocumentEnvelope(
        authoredProject.projectId,
        "scene",
        authoredProject.startupScene,
        "s5a-test-level-snapshot");
    if (!WriteDocumentEnvelope(
            authoredScenePath + ".rmeta",
            authoredSceneEnvelope,
            error))
    {
        return Fail("write snapshot fixture Scene identity: " + error);
    }
    if (!session.SaveScene(authoredScenePath))
        return Fail("save snapshot fixture Scene before attachment");

    auto& authoredScene = session.Scenes().GetScene();
    if (authoredScene.transforms.GetCount() != 1)
        return Fail("snapshot fixture does not contain exactly one barrel");
    const wi::ecs::Entity authoredBarrel = authoredScene.transforms.GetEntity(0);
    const StableId authoredBarrelId =
        PersistentEntityId(authoredScene, authoredBarrel);
    if (!IsValidStableId(authoredBarrelId))
        return Fail("snapshot fixture barrel has no persistent identity");

    const std::string snapshotScriptPath =
        "Content/Scripts/s5a_move_barrel.lua";
    const fs::path authoredScript =
        fs::u8path(authoredProject.rootPath) / fs::u8path(snapshotScriptPath);
    if (!WriteText(
            authoredScript,
            "if renegade and renegade.metadata then\n"
            " renegade.metadata({\n"
            "  schema_version=1,\n"
            "  name='S5A Move Barrel',\n"
            "  category='Tests',\n"
            "  role='SCRIPT',\n"
            "  properties={}\n"
            " })\n"
            "end\n"
            "return {\n"
            " on_start=function(self)\n"
            "  assert(self.entity~=nil)\n"
            "  assert(renegade.entity.get_name(self.entity)=='S5 Test Barrel')\n"
            "  local ok,err=renegade.transform.translate_local(self.entity,{x=4,y=0,z=0})\n"
            "  assert(ok and not err)\n"
            " end\n"
            "}\n",
            error))
    {
        return Fail("write owner-workflow movement script: " + error);
    }

    std::vector<ScriptAuthoringSource> sources;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    if (!session.Scripts().EnumerateProjectSources(
            ScriptPresentation::Script,
            sources,
            diagnostics,
            error))
    {
        return Fail("enumerate owner-workflow script source: " + error);
    }
    const ScriptAuthoringSource* authoredSource =
        FindSource(sources, snapshotScriptPath);
    if (authoredSource == nullptr)
        return Fail("owner-workflow movement script was not discoverable");

    StableId scriptInstanceId;
    if (!session.Scripts().AttachEntitySource(
            authoredBarrelId,
            *authoredSource,
            scriptInstanceId,
            error))
    {
        return Fail("attach owner-workflow movement script: " + error);
    }

    const fs::path authoritativeCompanion =
        fs::u8path(ScriptDocumentPathForScene(authoredScenePath));
    if (fs::exists(authoritativeCompanion))
    {
        return Fail(
            "owner-workflow regression accidentally saved the source .rscripts");
    }
    if (!session.Commands().IsDirty())
        return Fail("unsaved script attachment did not mark Studio dirty");

    const std::size_t undoBeforeSnapshot = session.Commands().UndoCount();
    const std::size_t redoBeforeSnapshot = session.Commands().RedoCount();
    const std::string scenePathBeforeSnapshot = session.Scenes().CurrentPath();

    TestLevelSnapshotService snapshots(
        session.Scenes(),
        session.Commands(),
        &session.Scripts());
    TestLevelSnapshot snapshot;
    if (!snapshots.Create(authoredProject, snapshot, error))
        return Fail("create scripted Test Level snapshot: " + error);

    const fs::path snapshotScene = fs::u8path(snapshot.scenePath);
    const fs::path snapshotCompanion =
        fs::u8path(ScriptDocumentPathForScene(snapshot.scenePath));
    const fs::path snapshotSource =
        fs::u8path(snapshot.sessionDirectory) / fs::u8path(snapshotScriptPath);
    if (!snapshot.IsRuntimeReady() ||
        !fs::is_regular_file(snapshotScene) ||
        !fs::is_regular_file(fs::u8path(snapshot.scenePath + ".rmeta")) ||
        !fs::is_regular_file(snapshotCompanion) ||
        !fs::is_regular_file(snapshotSource))
    {
        return Fail(
            "scripted Test Level snapshot did not contain the governed Runtime inputs");
    }

    const auto launch = ParseRuntimeLaunchArguments(
        {"dx12", "--project", snapshot.descriptorPath});
    if (!launch.succeeded)
        return Fail("scripted snapshot launch arguments: " + launch.message);
    const auto resolved = ResolveRuntimeProject(launch);
    if (!resolved.succeeded)
        return Fail("scripted snapshot project resolution: " + resolved.message);

    SceneService runtimeScenes;
    const auto loaded = LoadRuntimeProjectScene(runtimeScenes, resolved);
    if (!loaded.succeeded)
        return Fail("scripted snapshot Runtime Scene load: " + loaded.message);

    RuntimeScriptRuntime snapshotRuntime;
    error.clear();
    if (!snapshotRuntime.StartSceneFromCompanion(
            resolved.startupScenePath,
            authoredProject.projectId,
            runtimeScenes.GetScene(),
            snapshot.sessionDirectory,
            error))
    {
        return Fail("StartSceneFromCompanion for Test Level snapshot: " + error);
    }
    if (snapshotRuntime.ActiveInstanceCount() != 1 ||
        snapshotRuntime.DisabledInstanceCount() != 0 ||
        !snapshotRuntime.Diagnostics().empty())
    {
        std::string detail;
        if (!snapshotRuntime.Diagnostics().empty())
            detail = ": " + snapshotRuntime.Diagnostics().front().message;
        return Fail(
            "Test Level snapshot did not start exactly one healthy creator script" +
            detail);
    }

    EntityIdentityIndex runtimeIdentities;
    if (!runtimeIdentities.Build(runtimeScenes.GetScene(), error))
        return Fail("index scripted snapshot Runtime entities: " + error);
    const wi::ecs::Entity runtimeBarrel =
        runtimeIdentities.Resolve(authoredBarrelId);
    if (runtimeBarrel == wi::ecs::INVALID_ENTITY)
        return Fail("scripted snapshot barrel identity did not survive Runtime load");

    const auto* runtimeTransform =
        runtimeScenes.GetScene().transforms.GetComponent(runtimeBarrel);
    if (runtimeTransform == nullptr ||
        !Near(runtimeTransform->translation_local.x, 4.0f) ||
        !Near(runtimeTransform->translation_local.y, 0.0f) ||
        !Near(runtimeTransform->translation_local.z, 0.0f))
    {
        return Fail(
            "real Test Level companion path did not move the barrel on on_start");
    }
    const XMFLOAT3 runtimeWorld = runtimeTransform->GetPosition();
    if (!Near(runtimeWorld.x, 4.0f) ||
        !Near(runtimeWorld.y, 0.0f) ||
        !Near(runtimeWorld.z, 0.0f))
    {
        return Fail(
            "real Test Level companion path did not update the visible Wicked transform");
    }

    snapshotRuntime.StopScene();
    if (!snapshots.Cleanup(snapshot, error))
        return Fail("cleanup scripted Test Level snapshot: " + error);

    if (fs::exists(authoritativeCompanion) ||
        session.Scenes().CurrentPath() != scenePathBeforeSnapshot ||
        !session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBeforeSnapshot ||
        session.Commands().RedoCount() != redoBeforeSnapshot)
    {
        return Fail(
            "scripted Test Level snapshot changed authoritative Studio state");
    }

    fs::remove_all(root, ec);
    return 0;
}
