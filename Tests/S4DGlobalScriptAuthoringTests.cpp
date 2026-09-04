#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/StudioSession.h"

#include <chrono>
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
        std::cerr << "S4D GLOBAL SCRIPT test failed: " << message << '\n';
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
        scene.Entity_CreateTransform("S4D Global Fixture");
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
}

int main()
{
    bool ok = true;
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s4d-global-" + std::to_string(unique));
    const fs::path projectsRoot = root / "Projects";
    const fs::path templateScene = root / "Template.wiscene";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(projectsRoot, ec);
    ok = Expect(!ec, "create temporary root") && ok;
    ok = Expect(WriteScene(templateScene), "write WISCENE fixture") && ok;

    StudioSession session;
    session.Projects().Initialize((root / "editor-state.ini").generic_u8string());
    ok = Expect(
        session.Projects().CreateProject(
            projectsRoot.generic_u8string(),
            "S4DGlobal",
            templateScene.generic_u8string()),
        "stage project") && ok;

    const std::string scenePath = session.Projects().StartupScenePath();
    ok = Expect(session.LoadScene(scenePath), "adopt project Scene") && ok;
    const auto& project = session.Projects().CurrentProject();

    DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
        project.projectId,
        "scene",
        project.startupScene,
        "s4d-global-tests");
    std::string error;
    ok = Expect(
        WriteDocumentEnvelope(scenePath + ".rmeta", sceneEnvelope, error),
        "write Scene identity sidecar: " + error) && ok;
    ok = Expect(session.SaveScene(scenePath), "save Scene before globals") && ok;

    const fs::path projectRoot = fs::u8path(project.rootPath);
    const std::string globalPath = "Content/Scripts/level_weather.lua";
    ok = Expect(WriteText(projectRoot, globalPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({
        schema_version=1,
        name="Level Weather",
        category="Testing",
        role="GLOBAL SCRIPT",
        properties={{name="enabled_flag",label="Enabled Flag",type="boolean",default=true}}
    })
end
return {}
)LUA"), "write GLOBAL SCRIPT") && ok;

    std::vector<ScriptAuthoringSource> globals;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    error.clear();
    ok = Expect(
        session.Scripts().EnumerateProjectSources(
            ScriptPresentation::GlobalScript,
            globals,
            diagnostics,
            error),
        "enumerate GLOBAL SCRIPT sources: " + error) && ok;
    const ScriptAuthoringSource* source = FindSource(globals, globalPath);
    ok = Expect(source != nullptr, "GLOBAL SCRIPT source is discoverable") && ok;
    if (source == nullptr)
    {
        fs::remove_all(root, ec);
        return 1;
    }

    StableId firstId;
    StableId secondId;
    error.clear();
    ok = Expect(
        session.Scripts().AttachLevelSource(*source, firstId, error),
        "attach first GLOBAL SCRIPT: " + error) && ok;
    error.clear();
    ok = Expect(
        session.Scripts().AttachLevelSource(*source, secondId, error),
        "attach second GLOBAL SCRIPT instance: " + error) && ok;
    ok = Expect(firstId != secondId, "GLOBAL SCRIPT instances have unique IDs") && ok;

    auto attachments = session.Scripts().LevelAttachments(
        ScriptPresentation::GlobalScript);
    ok = Expect(attachments.size() == 2, "two GLOBAL SCRIPT instances attached") && ok;
    if (attachments.size() == 2)
    {
        ok = Expect(
            attachments[0]->scope == ScriptScope::Level &&
            attachments[0]->ownerEntityId.empty(),
            "GLOBAL SCRIPT uses Level scope without an entity owner") && ok;
        ok = Expect(
            attachments[0]->sourceId == attachments[1]->sourceId,
            "shared GLOBAL SCRIPT source reuses sourceId") && ok;
        ok = Expect(
            attachments[0]->properties.size() == 1 &&
            attachments[0]->properties[0].booleanValue,
            "GLOBAL SCRIPT receives S4A metadata defaults") && ok;
    }
    ok = Expect(session.Commands().IsDirty(), "GLOBAL attach marks shared history dirty") && ok;

    error.clear();
    ok = Expect(
        session.Scripts().SetAttachmentEnabled(firstId, false, error),
        "disable GLOBAL SCRIPT: " + error) && ok;
    ok = Expect(session.Commands().Undo(), "Undo restores GLOBAL enabled state") && ok;
    ok = Expect(session.Commands().Redo(), "Redo reapplies GLOBAL enabled state") && ok;

    attachments = session.Scripts().LevelAttachments(
        ScriptPresentation::GlobalScript);
    if (attachments.size() == 2)
    {
        error.clear();
        ok = Expect(
            session.Scripts().MoveAttachment(
                secondId, attachments[0]->order, error),
            "reorder GLOBAL SCRIPT: " + error) && ok;
        ok = Expect(session.Commands().Undo(), "Undo restores GLOBAL order") && ok;
        ok = Expect(session.Commands().Redo(), "Redo reapplies GLOBAL order") && ok;
    }

    error.clear();
    ok = Expect(
        session.Scripts().RemoveAttachment(secondId, error),
        "remove GLOBAL SCRIPT: " + error) && ok;
    ok = Expect(session.Commands().Undo(), "Undo restores removed GLOBAL SCRIPT") && ok;

    ok = Expect(session.SaveScene(scenePath), "save Scene plus GLOBAL companion") && ok;
    ok = Expect(
        fs::is_regular_file(fs::u8path(ScriptDocumentPathForScene(scenePath))),
        "GLOBAL SCRIPT persisted to .rscripts") && ok;
    ok = Expect(!session.Commands().IsDirty(), "GLOBAL save marks history clean") && ok;

    ok = Expect(session.ReloadScene(), "reload Scene") && ok;
    error.clear();
    ok = Expect(session.Scripts().EnsureCurrent(error), "reload GLOBAL companion: " + error) && ok;
    attachments = session.Scripts().LevelAttachments(
        ScriptPresentation::GlobalScript);
    ok = Expect(attachments.size() == 2, "GLOBAL SCRIPT instances survive reopen") && ok;

    bool foundFirst = false;
    bool foundSecond = false;
    for (const auto* attachment : attachments)
    {
        foundFirst = foundFirst || attachment->scriptInstanceId == firstId;
        foundSecond = foundSecond || attachment->scriptInstanceId == secondId;
    }
    ok = Expect(foundFirst && foundSecond, "GLOBAL ScriptInstanceIds survive reopen") && ok;

    fs::remove_all(root, ec);
    if (!ok)
        return 1;
    std::cout << "PASS: S4D GLOBAL SCRIPT authoring\n";
    return 0;
}
