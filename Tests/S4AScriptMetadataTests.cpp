#include "renegade/bridge/ScriptMetadataService.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    bool Expect(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "S4A metadata test failed: " << message << '\n';
        return false;
    }

    bool HasCode(
        const ScriptMetadataEvaluationResult& result,
        const std::string& code)
    {
        for (const auto& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    bool WriteSource(
        const fs::path& root,
        const std::string& relativePath,
        const std::string& source)
    {
        const fs::path path = root / fs::u8path(relativePath);
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << source;
        return static_cast<bool>(stream);
    }

    std::string ValidActionSource()
    {
        return R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Door Controller",
        description = "Metadata-only authoring contract proof.",
        category = "Interaction",
        role = "ACTION",
        properties = {
            { name="enabled", label="Enabled", type="boolean", default=true },
            { name="count", label="Count", type="integer", default=2, min=0, max=10, step=1 },
            { name="speed", label="Speed", type="float", default=2.5, min=0.0, max=20.0, step=0.25 },
            { name="prompt", label="Prompt", type="string", default="Open" },
            { name="tint", label="Tint", type="colour", default={r=1.0,g=0.5,b=0.25,a=1.0} },
            { name="offset2", label="Offset 2D", type="vector2", default={x=1.0,y=2.0} },
            { name="offset3", label="Offset 3D", type="vector3", default={x=1.0,y=2.0,z=3.0} },
            { name="target", label="Target", type="entity" },
            { name="asset", label="Asset", type="asset" },
            { name="animation", label="Animation", type="animation" },
            { name="sound", label="Sound", type="audio" },
            { name="mode", label="Mode", type="enum", default="open", options={
                { value="closed", label="Closed" },
                { value="open", label="Open" },
            } },
        },
    })
end

error("gameplay body must never run during metadata evaluation")
return { on_start = function(self) end }
)LUA";
    }
}

int main()
{
    bool ok = true;
    const fs::path root = fs::temp_directory_path() /
        "renegade_s4a_script_metadata_tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "Content" / "Scripts", ec);
    ok = Expect(!ec, "create temporary project root") && ok;

    const std::string actionPath = "Content/Scripts/door_action.lua";
    ok = Expect(
        WriteSource(root, actionPath, ValidActionSource()),
        "write valid ACTION source") && ok;

    const auto valid = EvaluateScriptMetadata(
        root.u8string(), actionPath);
    ok = Expect(valid.succeeded, "valid ACTION metadata evaluates") && ok;
    ok = Expect(valid.diagnostics.empty(), "valid ACTION has no diagnostics") && ok;
    ok = Expect(valid.descriptor.schemaVersion == 1, "schema v1 persists") && ok;
    ok = Expect(valid.descriptor.name == "Door Controller", "name captured") && ok;
    ok = Expect(valid.descriptor.category == "Interaction", "category captured") && ok;
    ok = Expect(
        valid.descriptor.presentation == ScriptPresentation::Action,
        "ACTION presentation captured") && ok;
    ok = Expect(valid.descriptor.properties.size() == 12, "all 12 property types captured") && ok;
    if (valid.descriptor.properties.size() == 12)
    {
        ok = Expect(valid.descriptor.properties[0].name == "enabled", "property order 1") && ok;
        ok = Expect(valid.descriptor.properties[2].name == "speed", "property order 3") && ok;
        ok = Expect(valid.descriptor.properties[7].type == ScriptPropertyType::EntityReference,
            "entity property type") && ok;
        ok = Expect(valid.descriptor.properties[8].type == ScriptPropertyType::AssetReference,
            "asset property type") && ok;
        ok = Expect(valid.descriptor.properties[9].type == ScriptPropertyType::Animation,
            "animation property type") && ok;
        ok = Expect(valid.descriptor.properties[10].type == ScriptPropertyType::Audio,
            "audio property type") && ok;
        ok = Expect(valid.descriptor.properties[11].enumOptions.size() == 2,
            "enum options captured") && ok;
    }

    ScriptAttachment attachment;
    attachment.scope = ScriptScope::Entity;
    attachment.presentation = ScriptPresentation::Action;
    ScriptPropertyValue existingSpeed;
    existingSpeed.name = "speed";
    existingSpeed.type = ScriptPropertyType::Float;
    existingSpeed.numberValue = 9.0f;
    attachment.properties.push_back(existingSpeed);
    std::string applyError;
    ok = Expect(
        ApplyScriptMetadataDefaults(valid.descriptor, attachment, applyError),
        "apply metadata defaults") && ok;
    ok = Expect(attachment.properties.size() == 12,
        "defaults seed every declared property including unresolved refs") && ok;
    for (const auto& property : attachment.properties)
    {
        if (property.name == "speed")
            ok = Expect(std::fabs(property.numberValue - 9.0f) < 0.001f,
                "persisted property value wins over metadata default") && ok;
    }

    const std::string globalPath = "Content/Scripts/global.lua";
    ok = Expect(
        WriteSource(root, globalPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({ schema_version=1, name="Game Clock", category="Level", role="GLOBAL SCRIPT", properties={} })
end
return {}
)LUA"),
        "write global source") && ok;
    const auto global = EvaluateScriptMetadata(root.u8string(), globalPath);
    ok = Expect(global.succeeded, "GLOBAL SCRIPT metadata evaluates") && ok;
    ScriptAttachment globalAttachment;
    globalAttachment.scope = ScriptScope::Level;
    globalAttachment.presentation = ScriptPresentation::GlobalScript;
    ok = Expect(
        ApplyScriptMetadataDefaults(global.descriptor, globalAttachment, applyError),
        "GLOBAL SCRIPT defaults accept Level scope") && ok;

    const std::string badRolePath = "Content/Scripts/bad_role.lua";
    WriteSource(root, badRolePath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({ schema_version=1, name="Bad", category="Tests", role="BEHAVIOUR" })
end
return {}
)LUA");
    const auto badRole = EvaluateScriptMetadata(root.u8string(), badRolePath);
    ok = Expect(!badRole.succeeded && HasCode(badRole, "metadata.invalid_role"),
        "role vocabulary remains ACTION/SCRIPT/GLOBAL SCRIPT") && ok;

    const std::string duplicatePath = "Content/Scripts/duplicate.lua";
    WriteSource(root, duplicatePath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({ schema_version=1, name="Duplicate", category="Tests", role="SCRIPT", properties={
        {name="value",type="integer",default=1},
        {name="value",type="integer",default=2},
    } })
end
return {}
)LUA");
    const auto duplicate = EvaluateScriptMetadata(root.u8string(), duplicatePath);
    ok = Expect(!duplicate.succeeded && HasCode(duplicate, "metadata.duplicate_property"),
        "duplicate property identifiers rejected") && ok;

    const std::string refDefaultPath = "Content/Scripts/ref_default.lua";
    WriteSource(root, refDefaultPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({ schema_version=1, name="Ref", category="Tests", role="SCRIPT", properties={
        {name="target",type="entity",default="1234"},
    } })
end
return {}
)LUA");
    const auto refDefault = EvaluateScriptMetadata(root.u8string(), refDefaultPath);
    ok = Expect(!refDefault.succeeded && HasCode(refDefault, "metadata.reference_default_forbidden"),
        "raw source-authored reference defaults rejected") && ok;

    const std::string enumPath = "Content/Scripts/bad_enum.lua";
    WriteSource(root, enumPath, R"LUA(
if renegade and renegade.metadata then
    renegade.metadata({ schema_version=1, name="Enum", category="Tests", role="SCRIPT", properties={
        {name="mode",type="enum",default="missing",options={"one","two"}},
    } })
end
return {}
)LUA");
    const auto badEnum = EvaluateScriptMetadata(root.u8string(), enumPath);
    ok = Expect(!badEnum.succeeded && HasCode(badEnum, "metadata.enum_default_missing"),
        "enum default must resolve") && ok;

    const std::string syntaxPath = "Content/Scripts/syntax.lua";
    WriteSource(root, syntaxPath, "if then this is not Lua");
    const auto syntax = EvaluateScriptMetadata(root.u8string(), syntaxPath);
    ok = Expect(!syntax.succeeded && HasCode(syntax, "metadata.lua_syntax_error"),
        "syntax errors reported without gameplay execution") && ok;

    const std::string missingPath = "Content/Scripts/missing.lua";
    WriteSource(root, missingPath, "return { on_start=function(self) end }");
    const auto missing = EvaluateScriptMetadata(root.u8string(), missingPath);
    ok = Expect(!missing.succeeded && HasCode(missing, "metadata.declaration_missing"),
        "missing metadata declaration rejected by S4A evaluator") && ok;

    const std::string sandboxPath = "Content/Scripts/sandbox.lua";
    WriteSource(root, sandboxPath, R"LUA(
os.execute("must not exist")
if renegade and renegade.metadata then
    renegade.metadata({schema_version=1,name="Sandbox",category="Tests",role="SCRIPT"})
end
return {}
)LUA");
    const auto sandbox = EvaluateScriptMetadata(root.u8string(), sandboxPath);
    ok = Expect(!sandbox.succeeded && HasCode(sandbox, "metadata.evaluation_failed"),
        "metadata state exposes no os library") && ok;

    const std::string runawayPath = "Content/Scripts/runaway.lua";
    WriteSource(root, runawayPath, R"LUA(
while true do end
if renegade and renegade.metadata then
    renegade.metadata({schema_version=1,name="Runaway",category="Tests",role="SCRIPT"})
end
return {}
)LUA");
    const auto runaway = EvaluateScriptMetadata(
        root.u8string(), runawayPath,
        ScriptMetadataDefaultMemoryBudgetBytes,
        5000u);
    ok = Expect(!runaway.succeeded && HasCode(runaway, "metadata.evaluation_failed"),
        "metadata instruction budget stops runaway source") && ok;

    const auto traversal = EvaluateScriptMetadata(
        root.u8string(), "../outside.lua");
    ok = Expect(!traversal.succeeded && HasCode(traversal, "metadata.source_invalid"),
        "S2 governed source containment is reused") && ok;

    ScriptAttachment wrongRole;
    wrongRole.scope = ScriptScope::Entity;
    wrongRole.presentation = ScriptPresentation::Script;
    ok = Expect(
        !ApplyScriptMetadataDefaults(valid.descriptor, wrongRole, applyError),
        "attachment role must match metadata role") && ok;

    fs::remove_all(root, ec);
    return ok ? 0 : 1;
}
