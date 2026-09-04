#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <cmath>
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
        std::cerr << "S5 core gameplay Lua test failed: " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right) noexcept
    {
        return std::fabs(left - right) < 0.0001f;
    }

    bool WriteText(const fs::path& path, const std::string& text, std::string& error)
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
            error = "could not open Lua fixture";
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            error = "could not write Lua fixture";
            return false;
        }
        error.clear();
        return true;
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
    const XMFLOAT3 position = transform->translation_local;
    if (!Near(position.x, 1.5f) || !Near(position.y, 1.0f) || !Near(position.z, 5.0f))
        return Fail("governed Lua did not move the barrel through the S5A transform API");

    runtime.StopScene();
    std::error_code ec;
    fs::remove_all(root, ec);
    return 0;
}
