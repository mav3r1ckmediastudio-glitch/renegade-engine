#include "renegade/bridge/ScriptDocumentService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <wiConfig.h>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int Fail(const std::string& message)
    {
        std::cerr << "S2 script document test failed: " << message << '\n';
        return 1;
    }

    bool Check(const bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "S2 script document check failed: " << message << '\n';
        return condition;
    }

    ScriptSourceBinding MakeSource(
        const std::string& path,
        const ScriptPresentation presentation)
    {
        ScriptSourceBinding source;
        source.sourceId = GenerateStableId();
        source.sourcePath = path;
        source.presentation = presentation;
        source.apiVersion = 1;
        source.unsafe = false;
        source.provenance.kind = ScriptProvenanceKind::Project;
        source.provenance.contentHash = "fixture-hash";
        source.capabilities = { "entity.read", "events.emit" };
        return source;
    }

    ScriptPropertyValue StringProperty(const std::string& name, const std::string& value)
    {
        ScriptPropertyValue property;
        property.name = name;
        property.type = ScriptPropertyType::String;
        property.textValue = value;
        return property;
    }
}

int main()
{
    using namespace renegade::bridge;

    const StableId projectId = GenerateStableId();
    const StableId sceneDocumentId = GenerateStableId();
    const StableId ownerA = GenerateStableId();
    const StableId ownerB = GenerateStableId();
    const StableId unresolvedEntity = GenerateStableId();

    ScriptDocument document = CreateScriptDocument(
        projectId,
        sceneDocumentId,
        "Content/Scenes/S2 Test.wiscene",
        "s2-tests");
    if (!Check(
            document.envelope.pathHint == "Content/Scenes/S2 Test.wiscene.rscripts",
            "script companion suffix/path contract"))
        return 1;

    std::string error;
    if (!ValidateScriptDocument(document, error))
        return Fail("empty document should validate: " + error);

    ScriptSourceBinding actionSource = MakeSource(
        "Content/Scripts/shared # action.lua",
        ScriptPresentation::Action);
    ScriptDependency module;
    module.kind = ScriptDependencyKind::ScriptModule;
    module.id = GenerateStableId();
    module.pathHint = "Content/Scripts/common.lua";
    actionSource.dependencies.push_back(module);

    ScriptAttachment first = CreateScriptAttachment(
        ScriptScope::Entity, ownerA, actionSource);
    const StableId firstInstance = first.scriptInstanceId;
    if (!AddScriptAttachment(document, first, error))
        return Fail("first ACTION attach failed: " + error);

    ScriptAttachment second = CreateScriptAttachment(
        ScriptScope::Entity, ownerA, actionSource);
    const StableId secondInstance = second.scriptInstanceId;
    if (!AddScriptAttachment(document, second, error))
        return Fail("shared source ACTION attach failed: " + error);
    if (!Check(
            document.attachments[0].sourceId == document.attachments[1].sourceId,
            "shared source keeps one sourceId"))
        return 1;

    ScriptSourceBinding scriptSource = MakeSource(
        "Content/Scripts/advanced.lua",
        ScriptPresentation::Script);
    ScriptAttachment advanced = CreateScriptAttachment(
        ScriptScope::Entity, ownerA, scriptSource);
    const StableId advancedInstance = advanced.scriptInstanceId;
    if (!AddScriptAttachment(document, advanced, error))
        return Fail("SCRIPT attach failed: " + error);

    ScriptSourceBinding globalSource = MakeSource(
        "Content/Scripts/level_global.lua",
        ScriptPresentation::GlobalScript);
    ScriptAttachment global = CreateScriptAttachment(
        ScriptScope::Level, {}, globalSource);
    if (!AddScriptAttachment(document, global, error))
        return Fail("GLOBAL SCRIPT attach failed: " + error);

    ScriptPropertyValue booleanProperty;
    booleanProperty.name = "enabled_by_default";
    booleanProperty.type = ScriptPropertyType::Boolean;
    booleanProperty.booleanValue = true;
    if (!SetScriptProperty(document, firstInstance, booleanProperty, error))
        return Fail("boolean property failed: " + error);

    ScriptPropertyValue integerProperty;
    integerProperty.name = "count";
    integerProperty.type = ScriptPropertyType::Integer;
    integerProperty.integerValue = 922337203685477000LL;
    if (!SetScriptProperty(document, firstInstance, integerProperty, error))
        return Fail("integer property failed: " + error);

    ScriptPropertyValue floatProperty;
    floatProperty.name = "speed";
    floatProperty.type = ScriptPropertyType::Float;
    floatProperty.numberValue = 3.25f;
    if (!SetScriptProperty(document, firstInstance, floatProperty, error))
        return Fail("float property failed: " + error);

    if (!SetScriptProperty(
            document,
            firstInstance,
            StringProperty("message", "  dialogue #1; keep spaces  "),
            error))
        return Fail("string property failed: " + error);

    ScriptPropertyValue colour;
    colour.name = "tint";
    colour.type = ScriptPropertyType::Colour;
    colour.x = 0.1f;
    colour.y = 0.2f;
    colour.z = 0.3f;
    colour.w = 0.9f;
    if (!SetScriptProperty(document, firstInstance, colour, error))
        return Fail("colour property failed: " + error);

    ScriptPropertyValue vector2;
    vector2.name = "uv";
    vector2.type = ScriptPropertyType::Vector2;
    vector2.x = -1.5f;
    vector2.y = 4.0f;
    if (!SetScriptProperty(document, firstInstance, vector2, error))
        return Fail("Vector2 property failed: " + error);

    ScriptPropertyValue vector3;
    vector3.name = "offset";
    vector3.type = ScriptPropertyType::Vector3;
    vector3.x = 1.0f;
    vector3.y = 2.0f;
    vector3.z = 3.0f;
    if (!SetScriptProperty(document, firstInstance, vector3, error))
        return Fail("Vector3 property failed: " + error);

    ScriptPropertyValue entityRef;
    entityRef.name = "target";
    entityRef.type = ScriptPropertyType::EntityReference;
    entityRef.referenceId = unresolvedEntity;
    entityRef.pathHint = "Content/Scenes/S2 Test.wiscene";
    if (!SetScriptProperty(document, firstInstance, entityRef, error))
        return Fail("EntityRef property failed: " + error);

    ScriptPropertyValue assetRef;
    assetRef.name = "asset";
    assetRef.type = ScriptPropertyType::AssetReference;
    assetRef.referenceId = GenerateStableId();
    assetRef.pathHint = "Content/Models/Crate.rasset";
    if (!SetScriptProperty(document, firstInstance, assetRef, error))
        return Fail("AssetRef property failed: " + error);

    ScriptPropertyValue animationRef = assetRef;
    animationRef.name = "animation";
    animationRef.type = ScriptPropertyType::Animation;
    if (!SetScriptProperty(document, firstInstance, animationRef, error))
        return Fail("Animation property failed: " + error);

    ScriptPropertyValue audioRef = assetRef;
    audioRef.name = "audio";
    audioRef.type = ScriptPropertyType::Audio;
    if (!SetScriptProperty(document, firstInstance, audioRef, error))
        return Fail("Audio property failed: " + error);

    ScriptPropertyValue enumValue;
    enumValue.name = "mode";
    enumValue.type = ScriptPropertyType::Enum;
    enumValue.textValue = "aggressive";
    if (!SetScriptProperty(document, firstInstance, enumValue, error))
        return Fail("Enum property failed: " + error);

    if (!MoveScriptAttachment(document, advancedInstance, 0, error))
        return Fail("attachment reorder failed: " + error);
    const auto* moved = FindScriptAttachment(document, advancedInstance);
    if (!Check(moved != nullptr && moved->order == 0, "reorder did not become authoritative"))
        return 1;

    StableId duplicateInstance;
    if (!DuplicateScriptInstance(document, firstInstance, duplicateInstance, error))
        return Fail("script instance duplication failed: " + error);
    const auto* duplicate = FindScriptAttachment(document, duplicateInstance);
    const auto* original = FindScriptAttachment(document, firstInstance);
    if (!Check(
            duplicate != nullptr && original != nullptr &&
            duplicate->scriptInstanceId != original->scriptInstanceId &&
            duplicate->sourceId == original->sourceId,
            "duplicate must get new ScriptInstanceId while sharing sourceId"))
        return 1;

    std::vector<StableId> duplicatedEntityInstances;
    if (!DuplicateEntityScriptAttachments(
            document, ownerA, ownerB, duplicatedEntityInstances, error))
        return Fail("entity script duplication failed: " + error);
    if (!Check(!duplicatedEntityInstances.empty(), "entity duplication returned no new IDs"))
        return 1;
    for (const auto& id : duplicatedEntityInstances)
    {
        const auto* attachment = FindScriptAttachment(document, id);
        if (!Check(
                attachment != nullptr &&
                attachment->ownerEntityId == ownerB &&
                attachment->scope == ScriptScope::Entity,
                "duplicated entity attachment owner/identity mismatch"))
            return 1;
    }

    ScriptAttachment conflictingPath = CreateScriptAttachment(
        ScriptScope::Entity,
        ownerA,
        MakeSource("Content/Scripts/shared # action.lua", ScriptPresentation::Action));
    ScriptDocument conflictDocument = document;
    if (AddScriptAttachment(conflictDocument, conflictingPath, error))
        return Fail("same source path with different sourceId should be rejected");

    ScriptSourceBinding conflictingId = actionSource;
    conflictingId.sourcePath = "Content/Scripts/different.lua";
    ScriptAttachment conflictingSourceId = CreateScriptAttachment(
        ScriptScope::Entity, ownerA, conflictingId);
    conflictDocument = document;
    if (AddScriptAttachment(conflictDocument, conflictingSourceId, error))
        return Fail("same sourceId with conflicting source authority should be rejected");

    ScriptAttachment invalidGlobal = CreateScriptAttachment(
        ScriptScope::Entity, ownerA, globalSource);
    conflictDocument = document;
    if (AddScriptAttachment(conflictDocument, invalidGlobal, error))
        return Fail("GLOBAL SCRIPT should be rejected on entity scope");

    ScriptAttachment prematureGame = CreateScriptAttachment(
        ScriptScope::Game, {}, globalSource);
    conflictDocument = document;
    if (AddScriptAttachment(conflictDocument, prematureGame, error))
        return Fail("schema-v1 Game scope should remain reserved");

    wi::scene::Scene scene;
    const wi::ecs::Entity ownerEntityA = scene.Entity_CreateTransform("Owner A");
    const wi::ecs::Entity ownerEntityB = scene.Entity_CreateTransform("Owner B");
    if (!AssignPersistentEntityId(scene, ownerEntityA, ownerA, error) ||
        !AssignPersistentEntityId(scene, ownerEntityB, ownerB, error))
        return Fail("could not establish persistent Scene owner IDs: " + error);
    if (!ValidateScriptDocumentAgainstScene(document, scene, error))
        return Fail("Scene-bound validation failed: " + error);
    if (!Check(
            FindScriptAttachment(document, firstInstance) != nullptr,
            "unresolved EntityRef should remain retained"))
        return 1;

    CommandService commands;
    auto disable = MakeSetScriptEnabledCommand(document, firstInstance, false, error);
    if (!disable || !commands.Execute(std::move(disable)))
        return Fail("CommandService enable edit failed: " + error);
    if (!Check(!FindScriptAttachment(document, firstInstance)->enabled, "command did not disable instance"))
        return 1;
    if (!commands.Undo() ||
        !Check(FindScriptAttachment(document, firstInstance)->enabled, "Undo did not restore enabled state"))
        return 1;
    if (!commands.Redo() ||
        !Check(!FindScriptAttachment(document, firstInstance)->enabled, "Redo did not restore disabled state"))
        return 1;
    if (!commands.Undo())
        return Fail("final Undo failed");

    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s2-script-tests-" + GenerateStableId());
    const fs::path sceneFolder = root / "Content" / "Scenes";
    const fs::path scriptFolder = root / "Content" / "Scripts";
    std::error_code pathError;
    fs::create_directories(sceneFolder, pathError);
    fs::create_directories(scriptFolder, pathError);
    if (pathError)
        return Fail("could not create fixture folders: " + pathError.message());

    const fs::path validSource = scriptFolder / "shared # action.lua";
    {
        std::ofstream stream(validSource, std::ios::binary | std::ios::trunc);
        stream << "return { on_start = function(self) end }\n";
    }
    if (!ValidateProjectScriptSource(
            root.generic_u8string(),
            "Content/Scripts/shared # action.lua",
            error))
        return Fail("valid governed Lua source was rejected: " + error);
    if (ValidateProjectScriptSource(
            root.generic_u8string(),
            "Content/Scripts/../outside.lua",
            error))
        return Fail("source traversal should be rejected");
    if (ValidateProjectScriptSource(
            root.generic_u8string(),
            "Content/Other/not-script.lua",
            error))
        return Fail("source outside Content/Scripts should be rejected");
    if (ValidateProjectScriptSource(
            root.generic_u8string(),
            "Content/Scripts/shared # action.lua",
            error,
            8))
        return Fail("source size bound should reject oversized fixture");

    const fs::path companion = root / fs::u8path(
        ScriptDocumentPathHintForScene(document.scenePathHint));
    fs::create_directories(companion.parent_path(), pathError);
    if (!WriteScriptDocument(companion.generic_u8string(), document, error))
        return Fail("script document transactional write failed: " + error);

    ScriptDocument roundTrip;
    if (!ReadScriptDocument(
            companion.generic_u8string(),
            projectId,
            sceneDocumentId,
            roundTrip,
            error))
        return Fail("script document round-trip failed: " + error);
    if (!Check(
            FindScriptAttachment(roundTrip, firstInstance) != nullptr &&
            FindScriptAttachment(roundTrip, duplicateInstance) != nullptr,
            "save/reopen did not retain ScriptInstanceIds"))
        return 1;
    const auto* roundTripFirst = FindScriptAttachment(roundTrip, firstInstance);
    if (roundTripFirst == nullptr)
        return Fail("round-trip first attachment missing");
    const auto message = std::find_if(
        roundTripFirst->properties.begin(), roundTripFirst->properties.end(),
        [](const ScriptPropertyValue& property) { return property.name == "message"; });
    if (!Check(
            message != roundTripFirst->properties.end() &&
            message->textValue == "  dialogue #1; keep spaces  ",
            "encoded property text did not round-trip exactly"))
        return 1;

    {
        wi::config::File future;
        if (!future.Open(companion.generic_u8string()))
            return Fail("could not reopen companion for forward-compat fixture");
        static_cast<wi::config::Section&>(future).Set("future_root", "root-value");
        future.GetSection("document").Set("future_document", "document-value");
        future.GetSection("script_document").Set("future_script", "script-value");
        future.GetSection("attachment_0").Set("future_attachment", "attachment-value");
        future.GetSection("future_section").Set("future_key", "future-value");
        future.Commit();
    }

    ScriptDocument futureRoundTrip;
    if (!ReadScriptDocument(
            companion.generic_u8string(),
            projectId,
            sceneDocumentId,
            futureRoundTrip,
            error))
        return Fail("forward-compatible read failed: " + error);
    if (!WriteScriptDocument(companion.generic_u8string(), futureRoundTrip, error))
        return Fail("forward-compatible rewrite failed: " + error);
    {
        wi::config::File future;
        if (!future.Open(companion.generic_u8string()))
            return Fail("could not inspect forward-compatible rewrite");
        if (!Check(
                static_cast<const wi::config::Section&>(future).GetText("future_root") == "root-value" &&
                future.GetSection("document").GetText("future_document") == "document-value" &&
                future.GetSection("script_document").GetText("future_script") == "script-value" &&
                future.GetSection("attachment_0").GetText("future_attachment") == "attachment-value" &&
                future.GetSection("future_section").GetText("future_key") == "future-value",
                "unknown fields/sections were not preserved"))
            return 1;
    }

    const StableId sentinelDocumentId = futureRoundTrip.envelope.documentId;
    const fs::path malformed = root / "Content" / "Scenes" / "malformed.wiscene.rscripts";
    {
        std::ofstream stream(malformed, std::ios::binary | std::ios::trunc);
        stream << "not_a_renegade_document = true\n";
    }
    if (ReadScriptDocument(
            malformed.generic_u8string(),
            projectId,
            sceneDocumentId,
            futureRoundTrip,
            error))
        return Fail("malformed script companion should be rejected");
    if (!Check(
            futureRoundTrip.envelope.documentId == sentinelDocumentId,
            "failed read mutated the previously valid document"))
        return 1;

    fs::remove_all(root, pathError);
    std::cout << "S2 script document/source model tests passed\n";
    return 0;
}
