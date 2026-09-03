#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int Fail(const std::string& message)
    {
        std::cerr << "S3 governed Lua runtime test failed: " << message << '\n';
        return 1;
    }

    bool Check(const bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "S3 governed Lua check failed: " << message << '\n';
        return condition;
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
            error = ec.message();
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "could not open fixture";
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            error = "could not write fixture";
            return false;
        }
        error.clear();
        return true;
    }

    ScriptSourceBinding Source(
        const std::string& path,
        const ScriptPresentation presentation = ScriptPresentation::Script)
    {
        ScriptSourceBinding source;
        source.sourceId = GenerateStableId();
        source.sourcePath = path;
        source.presentation = presentation;
        source.apiVersion = RuntimeScriptRuntime::ApiVersion;
        source.provenance.kind = ScriptProvenanceKind::Project;
        source.provenance.contentHash = "s3-fixture";
        return source;
    }

    bool Add(
        ScriptDocument& document,
        const ScriptScope scope,
        const StableId& owner,
        const ScriptSourceBinding& source,
        std::string& error)
    {
        return AddScriptAttachment(
            document,
            CreateScriptAttachment(scope, owner, source),
            error);
    }

    std::size_t CountDiagnostic(
        const RuntimeScriptRuntime& runtime,
        const std::string& callback,
        const std::string& needle)
    {
        std::size_t count = 0;
        for (const auto& diagnostic : runtime.Diagnostics())
        {
            if (diagnostic.callback == callback &&
                diagnostic.message.find(needle) != std::string::npos)
            {
                ++count;
            }
        }
        return count;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s3-runtime-" + GenerateStableId());
    const fs::path scripts = root / "Content" / "Scripts";
    const fs::path scenes = root / "Content" / "Scenes";
    std::error_code ec;
    fs::create_directories(scripts, ec);
    fs::create_directories(scenes, ec);
    if (ec)
        return Fail("fixture folders: " + ec.message());

    std::string error;
    const auto writeScript = [&](const char* name, const char* text)
    {
        return WriteText(scripts / name, text, error);
    };

    if (!writeScript(
            "safe.lua",
            "return {\n"
            " on_start=function(self)\n"
            "  assert(os==nil and io==nil and package==nil and debug==nil)\n"
            "  assert(dofile==nil and loadfile==nil and load==nil)\n"
            "  assert(math and string and table and utf8)\n"
            "  assert(self.entity~=nil and renegade.entity.is_valid(self.entity))\n"
            "  assert(self.properties.speed==3.5)\n"
            "  assert(tostring(self.properties.asset)=='AssetRef')\n"
            "  assert(self.properties.target~=nil)\n"
            "  assert(not renegade.entity.is_valid(self.properties.target))\n"
            " end,\n"
            " on_update=function(self,dt)\n"
            "  counter=(counter or 0)+1\n"
            "  if counter~=1 then error('instance environment leaked') end\n"
            " end\n"
            "}\n") ||
        !writeScript(
            "global.lua",
            "return { on_start=function(self) "
            "if self.entity~=nil then error('global entity was not nil') end end }\n") ||
        !writeScript("noop.lua", "return {}\n") ||
        !writeScript("common.lua", "return {answer=42}\n") ||
        !writeScript(
            "module_user.lua",
            "local common=require('common')\n"
            "return {on_start=function(self) "
            "if common.answer~=42 then error('module mismatch') end end}\n") ||
        !writeScript(
            "undeclared.lua",
            "local missing=require('not_declared')\nreturn {}\n") ||
        !writeScript(
            "syntax.lua",
            "return {on_start=function(self) this is not lua end}\n") ||
        !writeScript(
            "update_error.lua",
            "return {on_update=function(self,dt) error('update-hit') end}\n") ||
        !writeScript(
            "budget.lua",
            "return {on_update=function(self,dt) while true do end end}\n") ||
        !writeScript(
            "pause.lua",
            "return {on_pause=function(self) error('pause-hit') end}\n") ||
        !writeScript(
            "resume.lua",
            "return {on_resume=function(self) error('resume-hit') end}\n") ||
        !writeScript(
            "reset.lua",
            "return {on_reset=function(self) error('reset-hit') end}\n"))
    {
        return Fail("script fixtures: " + error);
    }

    const StableId projectId = GenerateStableId();
    const StableId sceneDocumentId = GenerateStableId();
    const StableId ownerA = GenerateStableId();
    const StableId ownerB = GenerateStableId();
    const StableId unresolvedEntity = GenerateStableId();

    wi::scene::Scene scene;
    const auto entityA = scene.Entity_CreateTransform("S3 Owner A");
    const auto entityB = scene.Entity_CreateTransform("S3 Owner B");
    if (!AssignPersistentEntityId(scene, entityA, ownerA, error) ||
        !AssignPersistentEntityId(scene, entityB, ownerB, error))
    {
        return Fail("persistent owner IDs: " + error);
    }

    // Sandbox, typed context, Level global semantics, and independent _ENV.
    {
        ScriptDocument document = CreateScriptDocument(
            projectId,
            sceneDocumentId,
            "Content/Scenes/S3.wiscene",
            "s3-tests");
        ScriptSourceBinding safe = Source("Content/Scripts/safe.lua");

        ScriptAttachment first = CreateScriptAttachment(
            ScriptScope::Entity,
            ownerA,
            safe);
        ScriptPropertyValue speed;
        speed.name = "speed";
        speed.type = ScriptPropertyType::Float;
        speed.numberValue = 3.5f;
        first.properties.push_back(speed);
        ScriptPropertyValue asset;
        asset.name = "asset";
        asset.type = ScriptPropertyType::AssetReference;
        asset.referenceId = GenerateStableId();
        first.properties.push_back(asset);
        ScriptPropertyValue target;
        target.name = "target";
        target.type = ScriptPropertyType::EntityReference;
        target.referenceId = unresolvedEntity;
        first.properties.push_back(target);
        if (!AddScriptAttachment(document, first, error))
            return Fail("first safe attachment: " + error);

        ScriptAttachment second = CreateScriptAttachment(
            ScriptScope::Entity,
            ownerB,
            safe);
        second.properties = first.properties;
        if (!AddScriptAttachment(document, second, error) ||
            !Add(
                document,
                ScriptScope::Level,
                {},
                Source(
                    "Content/Scripts/global.lua",
                    ScriptPresentation::GlobalScript),
                error))
        {
            return Fail("shared/global attachment: " + error);
        }

        RuntimeScriptRuntime runtime;
        if (!runtime.StartScene(
                document,
                scene,
                root.generic_u8string(),
                error))
            return Fail("safe StartScene: " + error);
        if (!Check(runtime.IsInitialized(), "dedicated state not initialized") ||
            !Check(runtime.ActiveInstanceCount() == 3, "safe active count") ||
            !Check(runtime.DisabledInstanceCount() == 0, "safe disabled count") ||
            !Check(runtime.Diagnostics().empty(), "safe startup diagnostics"))
            return 1;

        runtime.Update(1.0f / 60.0f);
        if (!Check(runtime.Diagnostics().empty(), "instance _ENV isolation"))
            return 1;
        runtime.StopScene();
        runtime.Shutdown();
        if (!Check(!runtime.IsInitialized(), "Shutdown did not close Lua state"))
            return 1;
    }

    // Declared-only require() plus isolated load failures.
    {
        ScriptDocument document = CreateScriptDocument(
            projectId,
            sceneDocumentId,
            "Content/Scenes/S3.wiscene",
            "s3-tests");
        ScriptSourceBinding moduleUser = Source("Content/Scripts/module_user.lua");
        ScriptDependency common;
        common.kind = ScriptDependencyKind::ScriptModule;
        common.id = GenerateStableId();
        common.pathHint = "Content/Scripts/common.lua";
        moduleUser.dependencies.push_back(common);
        if (!Add(document, ScriptScope::Entity, ownerA, moduleUser, error) ||
            !Add(document, ScriptScope::Entity, ownerB,
                Source("Content/Scripts/undeclared.lua"), error) ||
            !Add(document, ScriptScope::Entity, ownerB,
                Source("Content/Scripts/syntax.lua"), error))
        {
            return Fail("module/failure attachments: " + error);
        }
        ScriptSourceBinding unsafeSource = Source("Content/Scripts/noop.lua");
        unsafeSource.unsafe = true;
        if (!Add(document, ScriptScope::Entity, ownerB, unsafeSource, error))
            return Fail("unsafe attachment: " + error);

        RuntimeScriptRuntime runtime;
        if (!runtime.StartScene(
                document,
                scene,
                root.generic_u8string(),
                error))
            return Fail("isolation StartScene: " + error);
        if (!Check(runtime.ActiveInstanceCount() == 1, "module active count") ||
            !Check(runtime.DisabledInstanceCount() == 3, "isolated load failures") ||
            !Check(CountDiagnostic(runtime, "load", "undeclared module") == 1,
                "undeclared require diagnostic") ||
            !Check(CountDiagnostic(runtime, "load", "Advanced/Unsafe") == 1,
                "unsafe diagnostic"))
            return 1;
    }

    // Optional on_update, callback isolation, and runaway-script budget.
    {
        ScriptDocument document = CreateScriptDocument(
            projectId,
            sceneDocumentId,
            "Content/Scenes/S3.wiscene",
            "s3-tests");
        if (!Add(document, ScriptScope::Entity, ownerA,
                Source("Content/Scripts/noop.lua"), error) ||
            !Add(document, ScriptScope::Entity, ownerA,
                Source("Content/Scripts/update_error.lua"), error) ||
            !Add(document, ScriptScope::Entity, ownerB,
                Source("Content/Scripts/budget.lua"), error))
            return Fail("update attachments: " + error);

        RuntimeScriptRuntime runtime;
        if (!runtime.StartScene(
                document,
                scene,
                root.generic_u8string(),
                error))
            return Fail("update StartScene: " + error);
        runtime.Update(1.0f / 60.0f);
        if (!Check(CountDiagnostic(runtime, "on_update", "update-hit") == 1,
                "on_update failure isolation") ||
            !Check(CountDiagnostic(runtime, "on_update", "instruction budget") == 1,
                "instruction budget") ||
            !Check(runtime.ActiveInstanceCount() == 1,
                "optional on_update instance remains active"))
            return 1;
    }

    // Pause, resume and reset lifecycle dispatch.
    {
        ScriptDocument document = CreateScriptDocument(
            projectId,
            sceneDocumentId,
            "Content/Scenes/S3.wiscene",
            "s3-tests");
        if (!Add(document, ScriptScope::Entity, ownerA,
                Source("Content/Scripts/pause.lua"), error) ||
            !Add(document, ScriptScope::Entity, ownerA,
                Source("Content/Scripts/resume.lua"), error) ||
            !Add(document, ScriptScope::Entity, ownerB,
                Source("Content/Scripts/reset.lua"), error))
            return Fail("lifecycle attachments: " + error);

        RuntimeScriptRuntime runtime;
        if (!runtime.StartScene(
                document,
                scene,
                root.generic_u8string(),
                error))
            return Fail("lifecycle StartScene: " + error);
        runtime.Pause();
        runtime.Resume();
        runtime.ResetScene();
        if (!Check(CountDiagnostic(runtime, "on_pause", "pause-hit") == 1,
                "on_pause") ||
            !Check(CountDiagnostic(runtime, "on_resume", "resume-hit") == 1,
                "on_resume") ||
            !Check(CountDiagnostic(runtime, "on_reset", "reset-hit") == 1,
                "on_reset") ||
            !Check(!runtime.IsRunning(), "ResetScene closes generation"))
            return 1;
    }

    // Adjacent .rscripts loading and legacy/no-companion compatibility.
    {
        const fs::path scenePath = scenes / "Companion.wiscene";
        if (!WriteText(scenePath, "fixture", error))
            return Fail("scene placeholder: " + error);
        DocumentEnvelope sceneEnvelope = CreateDocumentEnvelope(
            projectId,
            "scene",
            "Content/Scenes/Companion.wiscene",
            "s3-tests");
        if (!WriteDocumentEnvelope(
                scenePath.generic_u8string() + ".rmeta",
                sceneEnvelope,
                error))
            return Fail("scene identity: " + error);

        ScriptDocument companion = CreateScriptDocument(
            projectId,
            sceneEnvelope.documentId,
            "Content/Scenes/Companion.wiscene",
            "s3-tests");
        if (!Add(companion, ScriptScope::Entity, ownerA,
                Source("Content/Scripts/noop.lua"), error) ||
            !WriteScriptDocument(
                ScriptDocumentPathForScene(scenePath.generic_u8string()),
                companion,
                error))
            return Fail("companion fixture: " + error);

        RuntimeScriptRuntime runtime;
        if (!runtime.StartSceneFromCompanion(
                scenePath.generic_u8string(),
                projectId,
                scene,
                root.generic_u8string(),
                error))
            return Fail("companion start: " + error);
        if (!Check(runtime.ActiveInstanceCount() == 1,
                "companion script did not start"))
            return 1;

        const fs::path noScripts = scenes / "NoScripts.wiscene";
        if (!WriteText(noScripts, "fixture", error) ||
            !runtime.StartSceneFromCompanion(
                noScripts.generic_u8string(),
                projectId,
                scene,
                root.generic_u8string(),
                error))
            return Fail("no-companion compatibility: " + error);
        if (!Check(runtime.ActiveInstanceCount() == 0,
                "no-companion scene should have zero scripts"))
            return 1;
    }

    fs::remove_all(root, ec);
    std::cout << "S3 governed Lua runtime tests passed\n";
    return 0;
}
