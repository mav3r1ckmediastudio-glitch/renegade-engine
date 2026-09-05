#include "renegade/bridge/ScriptPropertyAuthoringService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    using namespace renegade::bridge;

    bool Finite(const float value) noexcept
    {
        return std::isfinite(static_cast<double>(value));
    }

    double ClampMetadataNumber(
        double value,
        const ScriptMetadataPropertyDescriptor& metadata)
    {
        if (metadata.hasMinimum)
            value = std::max(value, metadata.minimum);
        if (metadata.hasMaximum)
            value = std::min(value, metadata.maximum);
        return value;
    }

    double QuantizeMetadataNumber(
        double value,
        const ScriptMetadataPropertyDescriptor& metadata)
    {
        value = ClampMetadataNumber(value, metadata);
        if (metadata.hasStep && metadata.step > 0.0)
        {
            const double base = metadata.hasMinimum ? metadata.minimum : 0.0;
            value = base + std::round((value - base) / metadata.step) * metadata.step;
            value = ClampMetadataNumber(value, metadata);
        }
        return value;
    }

    bool SameValue(
        const ScriptPropertyValue& left,
        const ScriptPropertyValue& right) noexcept
    {
        if (left.name != right.name || left.type != right.type)
            return false;
        switch (left.type)
        {
        case ScriptPropertyType::Boolean:
            return left.booleanValue == right.booleanValue;
        case ScriptPropertyType::Integer:
            return left.integerValue == right.integerValue;
        case ScriptPropertyType::Float:
            return left.numberValue == right.numberValue;
        case ScriptPropertyType::String:
        case ScriptPropertyType::Enum:
            return left.textValue == right.textValue;
        case ScriptPropertyType::Colour:
            return left.x == right.x && left.y == right.y &&
                left.z == right.z && left.w == right.w;
        case ScriptPropertyType::Vector2:
            return left.x == right.x && left.y == right.y;
        case ScriptPropertyType::Vector3:
            return left.x == right.x && left.y == right.y && left.z == right.z;
        case ScriptPropertyType::EntityReference:
        case ScriptPropertyType::AssetReference:
        case ScriptPropertyType::Animation:
        case ScriptPropertyType::Audio:
            return left.referenceId == right.referenceId &&
                left.pathHint == right.pathHint;
        }
        return false;
    }
}

namespace renegade::bridge
{
    bool IsS4CEditableScriptPropertyType(
        const ScriptPropertyType type) noexcept
    {
        switch (type)
        {
        case ScriptPropertyType::Boolean:
        case ScriptPropertyType::Integer:
        case ScriptPropertyType::Float:
        case ScriptPropertyType::String:
        case ScriptPropertyType::Colour:
        case ScriptPropertyType::Vector2:
        case ScriptPropertyType::Vector3:
        case ScriptPropertyType::Enum:
            return true;
        case ScriptPropertyType::EntityReference:
        case ScriptPropertyType::AssetReference:
        case ScriptPropertyType::Animation:
        case ScriptPropertyType::Audio:
            return false;
        }
        return false;
    }

    bool NormalizeScriptPropertyForAuthoring(
        const ScriptMetadataPropertyDescriptor& metadata,
        ScriptPropertyValue& value,
        std::string& error)
    {
        if (metadata.name.empty())
        {
            error = "Script property metadata has no stable name.";
            return false;
        }
        if (value.type != metadata.type)
        {
            error = "Script property edit type does not match its metadata contract.";
            return false;
        }
        value.name = metadata.name;

        switch (metadata.type)
        {
        case ScriptPropertyType::Boolean:
        case ScriptPropertyType::String:
            break;

        case ScriptPropertyType::Integer:
        {
            // Preserve the exact 64-bit value when the metadata declares no
            // numeric constraint. Converting an unconstrained integer through
            // double would lose precision near the ends of int64_t.
            if (metadata.hasMinimum || metadata.hasMaximum || metadata.hasStep)
            {
                double number = static_cast<double>(value.integerValue);
                number = QuantizeMetadataNumber(number, metadata);
                value.integerValue =
                    static_cast<std::int64_t>(std::llround(number));
            }
            break;
        }

        case ScriptPropertyType::Float:
        {
            if (!Finite(value.numberValue))
            {
                error = "Script float property must be finite.";
                return false;
            }
            const double number = QuantizeMetadataNumber(
                static_cast<double>(value.numberValue), metadata);
            value.numberValue = static_cast<float>(number);
            break;
        }

        case ScriptPropertyType::Colour:
            if (!Finite(value.x) || !Finite(value.y) ||
                !Finite(value.z) || !Finite(value.w))
            {
                error = "Script colour components must be finite.";
                return false;
            }
            value.x = std::clamp(value.x, 0.0f, 1.0f);
            value.y = std::clamp(value.y, 0.0f, 1.0f);
            value.z = std::clamp(value.z, 0.0f, 1.0f);
            value.w = std::clamp(value.w, 0.0f, 1.0f);
            break;

        case ScriptPropertyType::Vector2:
            if (!Finite(value.x) || !Finite(value.y))
            {
                error = "Script Vector2 components must be finite.";
                return false;
            }
            break;

        case ScriptPropertyType::Vector3:
            if (!Finite(value.x) || !Finite(value.y) || !Finite(value.z))
            {
                error = "Script Vector3 components must be finite.";
                return false;
            }
            break;

        case ScriptPropertyType::Enum:
        {
            const auto option = std::find_if(
                metadata.enumOptions.begin(), metadata.enumOptions.end(),
                [&](const ScriptMetadataEnumOption& candidate)
                {
                    return candidate.value == value.textValue;
                });
            if (option == metadata.enumOptions.end())
            {
                error = "Script enum value is not one of the declared metadata options.";
                return false;
            }
            break;
        }

        case ScriptPropertyType::EntityReference:
        case ScriptPropertyType::AssetReference:
        case ScriptPropertyType::Animation:
        case ScriptPropertyType::Audio:
            error = "Reference-backed script properties are authored by S4D pickers.";
            return false;
        }

        error.clear();
        return true;
    }

    bool CommitScriptPropertyAuthoringEdit(
        ScriptAuthoringService& scripts,
        CommandService& commands,
        const StableId& scriptInstanceId,
        const ScriptMetadataPropertyDescriptor& metadata,
        ScriptPropertyValue value,
        std::string& error)
    {
        if (!IsValidStableId(scriptInstanceId))
        {
            error = "Script property edit requires a valid ScriptInstanceId.";
            return false;
        }
        if (!NormalizeScriptPropertyForAuthoring(metadata, value, error))
            return false;
        if (!scripts.EnsureCurrent(error))
            return false;

        ScriptDocument* document = scripts.Document();
        if (document == nullptr)
        {
            error = "No scripting document is loaded for property authoring.";
            return false;
        }
        const ScriptAttachment* attachment =
            FindScriptAttachment(*document, scriptInstanceId);
        if (attachment == nullptr)
        {
            error = "Script property owner is no longer attached.";
            return false;
        }

        const auto existing = std::find_if(
            attachment->properties.begin(), attachment->properties.end(),
            [&](const ScriptPropertyValue& candidate)
            {
                return candidate.name == metadata.name;
            });
        if (existing != attachment->properties.end() && SameValue(*existing, value))
        {
            error.clear();
            return true;
        }

        auto command = MakeSetScriptPropertyCommand(
            *document,
            scriptInstanceId,
            std::move(value),
            error);
        if (!command)
            return false;
        if (!commands.Execute(std::move(command)))
        {
            error = "Script property edit could not be committed to Undo/Redo history.";
            return false;
        }
        error.clear();
        return true;
    }
}
