#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptMetadataService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    class CommandService;
    class ProjectService;
    class SceneService;
    class ScriptAuthoringService;

    struct ScriptReferenceOption
    {
        StableId referenceId;
        std::string pathHint;
        std::string label;
        bool resolved = true;
    };

    [[nodiscard]] bool IsS4DReferencePropertyType(
        ScriptPropertyType type) noexcept;

    // Pure projections used by Studio and headless acceptance. Entity and
    // Animation references are Scene-owned persistent identities; Asset and
    // Audio references are LC01 asset-registry identities. No Wicked runtime
    // entity IDs are ever exposed through this authoring surface.
    [[nodiscard]] bool BuildSceneScriptReferenceOptions(
        const SceneService& scenes,
        ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error);
    [[nodiscard]] bool BuildRegistryScriptReferenceOptions(
        const AssetRegistry& registry,
        ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error);
    [[nodiscard]] bool EnumerateScriptReferenceOptions(
        const SceneService& scenes,
        const ProjectService& projects,
        ScriptPropertyType type,
        std::vector<ScriptReferenceOption>& options,
        std::string& error);

    // Converts one resolved picker choice (or the explicit empty choice) into
    // the S2 property representation. An unresolved existing value remains
    // valid persisted state, but cannot be manufactured as a new picker choice.
    [[nodiscard]] bool BuildScriptReferenceProperty(
        const ScriptMetadataPropertyDescriptor& metadata,
        const ScriptReferenceOption& option,
        ScriptPropertyValue& value,
        std::string& error);

    // S4D keeps the existing S2 command boundary authoritative. Reference
    // authoring participates in the same Undo/Redo and dirty-state history as
    // every other scripting edit.
    [[nodiscard]] bool CommitScriptReferenceAuthoringEdit(
        ScriptAuthoringService& scripts,
        CommandService& commands,
        const StableId& scriptInstanceId,
        const ScriptMetadataPropertyDescriptor& metadata,
        const ScriptReferenceOption& option,
        std::string& error);
}
