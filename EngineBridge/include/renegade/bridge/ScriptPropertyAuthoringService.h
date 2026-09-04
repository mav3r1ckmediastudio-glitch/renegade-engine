#pragma once

#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptMetadataService.h"

#include <string>

namespace renegade::bridge
{
    class CommandService;

    [[nodiscard]] bool IsS4CEditableScriptPropertyType(
        ScriptPropertyType type) noexcept;

    [[nodiscard]] bool NormalizeScriptPropertyForAuthoring(
        const ScriptMetadataPropertyDescriptor& metadata,
        ScriptPropertyValue& value,
        std::string& error);

    [[nodiscard]] bool CommitScriptPropertyAuthoringEdit(
        ScriptAuthoringService& scripts,
        CommandService& commands,
        const StableId& scriptInstanceId,
        const ScriptMetadataPropertyDescriptor& metadata,
        ScriptPropertyValue value,
        std::string& error);
}
