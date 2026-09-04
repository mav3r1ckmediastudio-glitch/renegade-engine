#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/StudioSession.h"

#include <chrono>
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

    bool Expect(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "S4B authoring test failed: " << message << '\n';
        return false;
    }

    bool WriteText(
        const fs::path& root,
        const std::string& relativePath,
        const std::string& text)
    {
        const fs::path path = root / fs::u8path(relativePath);
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
        return static_cast<bool>(stream);
    }

    bool WriteScene(const fs::path& path)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;

        wi::scene::Scene scene;
        scene.Entity_CreateTransform("S4B Script Owner");
        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen())
            return false;
        archive.SetCompressionEnabled(true);
        scene.Serialize(archive);
        const bool saved = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        return saved && fs::is_regular_file(path);
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

    const ScriptAttachment* FindAttachment(
        const ScriptDocument* document,
        const StableId& id)
    {
        return document == nullptr ? nullptr : FindScriptAttachment(*document, id);
    }
}

int main()
{
    bool ok = true;
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s4b-authoring-" + std::to_string(unique));
    const fs::path projectsRoot = root / "Projects";
    const fs::path templateScene = root / "Template.wiscene";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(projectsRoot, ec);
    ok = Expect(!ec, "create temporary root") && ok;
    ok = Expect(WriteScene(templateScene), "write headless WISCENE template") && ok;

    StudioSession session;
    session.Projects().Initialize((root / "editor-state.ini").generic_u8string());
    ok = Expect(
        session.Projects().CreateProject(
            projectsRoot.generic_u8string(),
            "S4BAuthoring",
            templateScene.generic_u8string()),
        "stage project") && ok;

    const std::string scenePath = session.Projects().StartupScenePath();
    ok = Expect(!scenePath.empty(), "staged project exposes startup Scene") && ok;
    ok = Expect(session.LoadScene(scenePath), "adopt project and startup Scene") && ok;
    ok = Expect(session.Projects().HasProject(), "project became authoritative") && ok;

    const auto& project = session.Projects().CurrentProject();
    DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
        project.projectId,
        "scene",
        project.startupScene,
        "s4b-authoring-tests");
    std::string error;
    ok = Expect(
        WriteDocumentEnvelope(scenePath + ".rmeta", sceneEnvelope, error),
        "write Scene identity sidecar: " + error) && ok;

    // First save gives the authored entity its persistent Renegade ID. With no
    // scripting state loaded yet, S4B must not emit an empty companion.
    ok = Expect(session.SaveScene(scenePath), "save Scene before script attachment") && ok;
    ok = Expect(
        !fs::exists(fs::u8path(ScriptDocumentPathForScene(scenePath))),
        "unused Scene does not gain an empty .rscripts companion") && ok;

    auto& scene = session.Scenes().GetScene();
    ok = Expect(scene.transforms.GetCount() == 1, "fixture has one script owner") && ok;
    const wi::ecs::Entity owner = scene.transforms.GetEntity(0);
    const StableId ownerId = PersistentEntityId(scene, owner);
    ok = Expect(IsValidStableId(ownerId), "Scene save assigned persistent owner ID") && ok;

    const fs::path projectRoot = fs::u8path(project.rootPath);
    const std::string actionPath = "Content/Scripts/open_door.lua";
    const std::string secondActionPath = "Content/Scripts/close_door.lua";
    const std::string scriptPath = "Content/Scripts/door_state.lua";
    const std::string badPath = "Content/Scripts/bad_metadata.lua";

    ok = Expect(WriteText(projectRoot, actionPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({
        schema_version=1,
        name="Open Door",
        category="Interaction",
        role="ACTION",
        properties={{name="speed",label="Speed",type="float",default=2.5}}
    })
end
return {}
)LUA"), "write first ACTION") && ok;
    ok = Expect(WriteText(projectRoot, secondActionPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({schema_version=1,name="Close Door",category="Interaction",role="ACTION",properties={}})
end
return {}
)LUA"), "write second ACTION") && ok;
    ok = Expect(WriteText(projectRoot, scriptPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({schema_version=1,name="Door State",category="Interaction",role="SCRIPT",properties={}})
end
return {}
)LUA"), "write SCRIPT") && ok;
    ok = Expect(WriteText(projectRoot, badPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({schema_version=1,name="Bad",category="Tests",role="BEHAVIOUR"})
end
return {}
)LUA"), "write malformed metadata source") && ok;

    std::vector<ScriptAuthoringSource> actions;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    error.clear();
    ok = Expect(
        session.Scripts().EnumerateProjectSources(
            ScriptPresentation::Action, actions, diagnostics, error),
        "enumerate ACTION sources: " + error) && ok;
    ok = Expect(actions.size() == 2, "only two valid ACTION sources are listed") && ok;
    ok = Expect(!diagnostics.empty(), "malformed metadata is reported but not listed") && ok;

    const ScriptAuthoringSource* actionSource = FindSource(actions, actionPath);
    ok = Expect(actionSource != nullptr, "first ACTION source is discoverable") && ok;

    std::vector<ScriptAuthoringSource> scripts;
    diagnostics.clear();
    error.clear();
    ok = Expect(
        session.Scripts().EnumerateProjectSources(
            ScriptPresentation::Script, scripts, diagnostics, error),
        "enumerate SCRIPT sources: " + error) && ok;
    const ScriptAuthoringSource* scriptSource = FindSource(scripts, scriptPath);
    ok = Expect(scriptSource != nullptr, "SCRIPT source is discoverable") && ok;

    // S4D extends the same governed source catalogue to GLOBAL SCRIPT. This
    // S4B fixture has no GLOBAL SCRIPT source, so discovery succeeds with an
    // empty compatible list while retaining any malformed-source diagnostics.
    std::vector<ScriptAuthoringSource> globals;
    diagnostics.clear();
    error.clear();
    ok = Expect(
        session.Scripts().EnumerateProjectSources(
            ScriptPresentation::GlobalScript, globals, diagnostics, error),
        "GLOBAL SCRIPT catalogue extension remains compatible: " + error) && ok;
    ok = Expect(globals.empty(), "S4B fixture has no GLOBAL SCRIPT sources") && ok;

    if (actionSource == nullptr || scriptSource == nullptr)
    {
        fs::remove_all(root, ec);
        return 1;
    }

    StableId actionOne;
    StableId actionTwo;
    StableId scriptOne;
    error.clear();
    ok = Expect(
        session.Scripts().AttachEntitySource(ownerId, *actionSource, actionOne, error),
        "attach first ACTION: " + error) && ok;
    error.clear();
    ok = Expect(
        session.Scripts().AttachEntitySource(ownerId, *actionSource, actionTwo, error),
        "attach second instance of same ACTION: " + error) && ok;
    error.clear();
    ok = Expect(
        session.Scripts().AttachEntitySource(ownerId, *scriptSource, scriptOne, error),
        "attach SCRIPT: " + error) && ok;

    auto actionAttachments = session.Scripts().EntityAttachments(
        ownerId, ScriptPresentation::Action);
    auto scriptAttachments = session.Scripts().EntityAttachments(
        ownerId, ScriptPresentation::Script);
    ok = Expect(actionAttachments.size() == 2, "two ACTION instances are attached") && ok;
    ok = Expect(scriptAttachments.size() == 1, "one SCRIPT instance is attached") && ok;
    ok = Expect(actionOne != actionTwo, "each attachment has a unique ScriptInstanceId") && ok;
    if (actionAttachments.size() == 2)
    {
        ok = Expect(
            actionAttachments[0]->sourceId == actionAttachments[1]->sourceId,
            "same governed source reuses one sourceId") && ok;
    }

    const ScriptAttachment* first = FindAttachment(session.Scripts().Document(), actionOne);
    ok = Expect(first != nullptr, "first ACTION resolves from live document") && ok;
    if (first != nullptr)
    {
        ok = Expect(first->properties.size() == 1, "S4A defaults seed attachment state") && ok;
        if (first->properties.size() == 1)
        {
            ok = Expect(
                first->properties[0].name == "speed" &&
                std::fabs(first->properties[0].numberValue - 2.5f) < 0.001f,
                "float metadata default persists into S2 property state") && ok;
        }
    }
    ok = Expect(session.Commands().IsDirty(), "script attachment participates in Scene dirty state") && ok;

    error.clear();
    ok = Expect(
        session.Scripts().SetAttachmentEnabled(actionOne, false, error),
        "disable ACTION: " + error) && ok;
    first = FindAttachment(session.Scripts().Document(), actionOne);
    ok = Expect(first != nullptr && !first->enabled, "disable state applied") && ok;
    ok = Expect(session.Commands().Undo(), "Undo restores script enabled state") && ok;
    first = FindAttachment(session.Scripts().Document(), actionOne);
    ok = Expect(first != nullptr && first->enabled, "Undo restored enabled=true") && ok;
    ok = Expect(session.Commands().Redo(), "Redo reapplies script enabled state") && ok;
    first = FindAttachment(session.Scripts().Document(), actionOne);
    ok = Expect(first != nullptr && !first->enabled, "Redo restored enabled=false") && ok;

    actionAttachments = session.Scripts().EntityAttachments(
        ownerId, ScriptPresentation::Action);
    ok = Expect(actionAttachments.size() == 2, "ACTION list survives enabled edit") && ok;
    if (actionAttachments.size() == 2)
    {
        const StableId later = actionAttachments[1]->scriptInstanceId;
        const std::uint32_t earlierOrder = actionAttachments[0]->order;
        error.clear();
        ok = Expect(
            session.Scripts().MoveAttachment(later, earlierOrder, error),
            "move later ACTION earlier in entity execution order: " + error) && ok;
        const auto reordered = session.Scripts().EntityAttachments(
            ownerId, ScriptPresentation::Action);
        ok = Expect(
            reordered.size() == 2 && reordered[0]->scriptInstanceId == later,
            "ACTION reorder follows authoritative S2 order") && ok;
        ok = Expect(session.Commands().Undo(), "Undo restores attachment order") && ok;
        ok = Expect(session.Commands().Redo(), "Redo reapplies attachment order") && ok;
    }

    error.clear();
    ok = Expect(
        session.Scripts().RemoveAttachment(scriptOne, error),
        "remove SCRIPT attachment: " + error) && ok;
    ok = Expect(
        session.Scripts().EntityAttachments(ownerId, ScriptPresentation::Script).empty(),
        "SCRIPT removal applied") && ok;
    ok = Expect(session.Commands().Undo(), "Undo restores removed SCRIPT") && ok;
    ok = Expect(
        session.Scripts().EntityAttachments(ownerId, ScriptPresentation::Script).size() == 1,
        "Undo restored SCRIPT attachment") && ok;

    const ScriptDocument beforeSave = *session.Scripts().Document();
    ok = Expect(session.SaveScene(scenePath), "save WISCENE and scripting companion") && ok;
    const std::string companionPath = ScriptDocumentPathForScene(scenePath);
    ok = Expect(fs::is_regular_file(fs::u8path(companionPath)), ".rscripts companion was written") && ok;
    ok = Expect(!session.Commands().IsDirty(), "successful combined save is clean") && ok;

    ScriptDocument persisted;
    error.clear();
    ok = Expect(
        ReadScriptDocument(
            companionPath,
            project.projectId,
            sceneEnvelope.documentId,
            persisted,
            error),
        "read saved scripting companion: " + error) && ok;
    ok = Expect(
        persisted.attachments.size() == beforeSave.attachments.size(),
        "saved companion retains every attachment") && ok;
    ok = Expect(
        FindScriptAttachment(persisted, actionOne) != nullptr &&
        FindScriptAttachment(persisted, actionTwo) != nullptr &&
        FindScriptAttachment(persisted, scriptOne) != nullptr,
        "save preserves ScriptInstanceIds") && ok;

    ok = Expect(session.ReloadScene(), "reopen saved Scene") && ok;
    error.clear();
    ok = Expect(session.Scripts().EnsureCurrent(error), "reopen scripting companion: " + error) && ok;
    const wi::ecs::Entity reopenedOwner = session.Scenes().GetScene().transforms.GetEntity(0);
    const StableId reopenedOwnerId = PersistentEntityId(
        session.Scenes().GetScene(), reopenedOwner);
    ok = Expect(reopenedOwnerId == ownerId, "Scene reopen preserves persistent owner ID") && ok;
    ok = Expect(
        session.Scripts().EntityAttachments(
            reopenedOwnerId, ScriptPresentation::Action).size() == 2 &&
        session.Scripts().EntityAttachments(
            reopenedOwnerId, ScriptPresentation::Script).size() == 1,
        "Scene reopen restores ACTION and SCRIPT attachments") && ok;

    actions.clear();
    diagnostics.clear();
    error.clear();
    ok = Expect(
        session.Scripts().EnumerateProjectSources(
            ScriptPresentation::Action, actions, diagnostics, error),
        "refresh ACTION sources after reopen: " + error) && ok;
    const ScriptAuthoringSource* reopenedAction = FindSource(actions, actionPath);
    const ScriptAttachment* persistedAction = FindScriptAttachment(
        *session.Scripts().Document(), actionOne);
    ok = Expect(
        reopenedAction != nullptr && persistedAction != nullptr &&
        reopenedAction->binding.sourceId == persistedAction->sourceId,
        "source discovery reuses persisted source authority") && ok;

    fs::remove_all(root, ec);
    return ok ? 0 : 1;
}
