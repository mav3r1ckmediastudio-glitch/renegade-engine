#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* ScriptDocumentFormat = "renegade-script-document";
    inline constexpr const char* ScriptDocumentType = "scene-scripts";
    inline constexpr const char* ScriptDocumentSuffix = ".rscripts";
    inline constexpr std::size_t MaximumScriptSourceBytes = 1024u * 1024u;

    enum class ScriptScope : std::uint8_t
    {
        Entity,
        Level,
        Game,
    };

    enum class ScriptPresentation : std::uint8_t
    {
        Action,
        Script,
        GlobalScript,
    };

    enum class ScriptPropertyType : std::uint8_t
    {
        Boolean,
        Integer,
        Float,
        String,
        Colour,
        Vector2,
        Vector3,
        EntityReference,
        AssetReference,
        Animation,
        Audio,
        Enum,
    };

    enum class ScriptDependencyKind : std::uint8_t
    {
        ScriptModule,
        Asset,
    };

    enum class ScriptProvenanceKind : std::uint8_t
    {
        Project,
        InstalledLibrary,
        PersonalLibrary,
    };

    [[nodiscard]] const char* ScriptScopeName(ScriptScope value) noexcept;
    [[nodiscard]] const char* ScriptPresentationName(
        ScriptPresentation value) noexcept;
    [[nodiscard]] const char* ScriptPresentationLabel(
        ScriptPresentation value) noexcept;
    [[nodiscard]] const char* ScriptPropertyTypeName(
        ScriptPropertyType value) noexcept;
    [[nodiscard]] const char* ScriptDependencyKindName(
        ScriptDependencyKind value) noexcept;
    [[nodiscard]] const char* ScriptProvenanceKindName(
        ScriptProvenanceKind value) noexcept;

    struct ScriptProvenance
    {
        ScriptProvenanceKind kind = ScriptProvenanceKind::Project;
        std::string libraryId;
        std::string libraryVersion;
        std::string contentHash;
        std::unordered_map<std::string, std::string> unknownFields;
    };

    struct ScriptDependency
    {
        ScriptDependencyKind kind = ScriptDependencyKind::ScriptModule;
        StableId id;
        std::string pathHint;
        bool optional = false;
        std::unordered_map<std::string, std::string> unknownFields;
    };

    struct ScriptPropertyValue
    {
        std::string name;
        ScriptPropertyType type = ScriptPropertyType::String;

        bool booleanValue = false;
        std::int64_t integerValue = 0;
        float numberValue = 0.0f;

        // Colour and vector values share compact storage. Vector2 uses x/y,
        // Vector3 uses x/y/z, and Colour uses x/y/z/w as RGBA.
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        // String and Enum use textValue. Reference-backed property types use
        // referenceId plus an optional project-relative path hint.
        std::string textValue;
        StableId referenceId;
        std::string pathHint;

        std::unordered_map<std::string, std::string> unknownFields;
    };

    struct ScriptSourceBinding
    {
        StableId sourceId;
        std::string sourcePath;
        ScriptPresentation presentation = ScriptPresentation::Script;
        std::uint32_t apiVersion = 1;
        bool unsafe = false;
        ScriptProvenance provenance;
        std::vector<ScriptDependency> dependencies;
        std::vector<std::string> capabilities;
    };

    struct ScriptAttachment
    {
        StableId scriptInstanceId;
        ScriptScope scope = ScriptScope::Entity;
        StableId ownerEntityId;

        StableId sourceId;
        std::string sourcePath;
        ScriptPresentation presentation = ScriptPresentation::Script;
        bool enabled = true;
        std::uint32_t order = 0;
        std::uint32_t apiVersion = 1;
        bool unsafe = false;

        std::vector<ScriptPropertyValue> properties;
        ScriptProvenance provenance;
        std::vector<ScriptDependency> dependencies;
        std::vector<std::string> capabilities;

        std::unordered_map<std::string, std::string> unknownFields;
    };

    struct ScriptDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        DocumentEnvelope envelope;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId sceneDocumentId;
        std::string scenePathHint;
        std::vector<ScriptAttachment> attachments;

        // Unknown fields/sections are retained on load and re-emitted on save
        // where they do not collide with schema-v1 authority.
        std::unordered_map<std::string, std::string> unknownRootFields;
        std::unordered_map<std::string, std::string> unknownDocumentFields;
        std::unordered_map<std::string, std::string>
            unknownScriptDocumentFields;
        std::unordered_map<
            std::string,
            std::unordered_map<std::string, std::string>> unknownSections;
    };

    [[nodiscard]] ScriptDocument CreateScriptDocument(
        const StableId& projectId,
        const StableId& sceneDocumentId,
        std::string scenePathHint,
        std::string generatorVersion);

    [[nodiscard]] std::string ScriptDocumentPathForScene(
        const std::string& scenePath);
    [[nodiscard]] std::string ScriptDocumentPathHintForScene(
        const std::string& scenePathHint);

    [[nodiscard]] ScriptSourceBinding CaptureScriptSourceBinding(
        const ScriptAttachment& attachment);

    [[nodiscard]] ScriptAttachment CreateScriptAttachment(
        ScriptScope scope,
        StableId ownerEntityId,
        const ScriptSourceBinding& source);

    [[nodiscard]] ScriptAttachment* FindScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId) noexcept;
    [[nodiscard]] const ScriptAttachment* FindScriptAttachment(
        const ScriptDocument& document,
        const StableId& scriptInstanceId) noexcept;

    [[nodiscard]] bool ValidateScriptDocument(
        const ScriptDocument& document,
        std::string& error);
    [[nodiscard]] bool ValidateScriptDocumentAgainstScene(
        const ScriptDocument& document,
        const wi::scene::Scene& scene,
        std::string& error);

    [[nodiscard]] bool WriteScriptDocument(
        const std::string& filePath,
        const ScriptDocument& document,
        std::string& error);
    [[nodiscard]] bool ReadScriptDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        const StableId& expectedSceneDocumentId,
        ScriptDocument& document,
        std::string& error);

    // Validates the governed authoring source itself without executing or
    // compiling Lua. Syntax/runtime ownership begins at S3.
    [[nodiscard]] bool ValidateProjectScriptSource(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& error,
        std::size_t maximumBytes = MaximumScriptSourceBytes);

    void NormalizeScriptAttachmentOrder(ScriptDocument& document);

    // In-memory edit primitives. Every mutation validates a candidate copy and
    // only commits it to the live document on success.
    [[nodiscard]] bool AddScriptAttachment(
        ScriptDocument& document,
        ScriptAttachment attachment,
        std::string& error);
    [[nodiscard]] bool RemoveScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string& error);
    [[nodiscard]] bool SetScriptAttachmentEnabled(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        bool enabled,
        std::string& error);
    [[nodiscard]] bool MoveScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::uint32_t newOrder,
        std::string& error);
    [[nodiscard]] bool ReplaceScriptSourceBinding(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const ScriptSourceBinding& source,
        std::string& error);
    [[nodiscard]] bool SetScriptProperty(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptPropertyValue property,
        std::string& error);
    [[nodiscard]] bool RemoveScriptProperty(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const std::string& propertyName,
        std::string& error);

    // Save/reopen retains ScriptInstanceId. Duplication never does: the new
    // attachment/entity receives fresh instance IDs while sharing sourceId.
    [[nodiscard]] bool DuplicateScriptInstance(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        StableId& duplicateScriptInstanceId,
        std::string& error);
    [[nodiscard]] bool DuplicateEntityScriptAttachments(
        ScriptDocument& document,
        const StableId& sourceOwnerEntityId,
        const StableId& duplicateOwnerEntityId,
        std::vector<StableId>& duplicateScriptInstanceIds,
        std::string& error);

    // Command factories make the S2 edit boundary directly usable by Studio's
    // existing CommandService without coupling the model to Inspector UI.
    [[nodiscard]] std::unique_ptr<ICommand> MakeAddScriptAttachmentCommand(
        ScriptDocument& document,
        ScriptAttachment attachment,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeRemoveScriptAttachmentCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeSetScriptEnabledCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        bool enabled,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeMoveScriptAttachmentCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::uint32_t newOrder,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeReplaceScriptSourceCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptSourceBinding source,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeSetScriptPropertyCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptPropertyValue property,
        std::string& error);
    [[nodiscard]] std::unique_ptr<ICommand> MakeRemoveScriptPropertyCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string propertyName,
        std::string& error);
}
