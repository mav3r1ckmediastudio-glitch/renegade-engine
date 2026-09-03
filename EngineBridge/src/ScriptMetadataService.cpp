#include "renegade/bridge/ScriptMetadataService.h"

#include <wiLua.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr std::size_t InstructionHookQuantum = 1000u;
    constexpr std::size_t MaximumMetadataProperties = 256u;
    constexpr std::size_t MaximumEnumOptions = 256u;
    constexpr const char* MetadataCapturedSentinel =
        "__RENEGADE_METADATA_CAPTURED__";

    thread_local std::size_t* activeMetadataInstructionBudget = nullptr;

    struct MetadataMemoryBudget
    {
        std::size_t usedBytes = 0;
        std::size_t limitBytes = ScriptMetadataDefaultMemoryBudgetBytes;
    };

    struct MetadataCaptureContext
    {
        ScriptMetadataEvaluationResult* result = nullptr;
        std::string sourcePath;
        bool captured = false;
    };

    void AddDiagnostic(
        MetadataCaptureContext& context,
        std::string code,
        std::string field,
        std::string message,
        const ScriptMetadataDiagnosticSeverity severity =
            ScriptMetadataDiagnosticSeverity::Error)
    {
        if (context.result == nullptr)
            return;
        context.result->diagnostics.push_back({
            severity,
            std::move(code),
            context.sourcePath,
            std::move(field),
            std::move(message),
            0,
        });
    }

    bool HasErrors(const ScriptMetadataEvaluationResult& result)
    {
        return std::any_of(
            result.diagnostics.begin(),
            result.diagnostics.end(),
            [](const ScriptMetadataDiagnostic& diagnostic)
            {
                return diagnostic.severity ==
                    ScriptMetadataDiagnosticSeverity::Error;
            });
    }

    void* MetadataAllocate(
        void* userData,
        void* pointer,
        const std::size_t oldSize,
        const std::size_t newSize)
    {
        auto* budget = static_cast<MetadataMemoryBudget*>(userData);
        if (budget == nullptr)
            return nullptr;

        if (newSize == 0)
        {
            if (pointer != nullptr)
            {
                budget->usedBytes = oldSize <= budget->usedBytes
                    ? budget->usedBytes - oldSize
                    : 0;
                std::free(pointer);
            }
            return nullptr;
        }

        const std::size_t accountedOld = pointer == nullptr ? 0 : oldSize;
        if (newSize > accountedOld)
        {
            const std::size_t growth = newSize - accountedOld;
            const std::size_t used =
                std::min(budget->usedBytes, budget->limitBytes);
            if (growth > budget->limitBytes - used)
                return nullptr;
        }

        void* replacement = std::realloc(pointer, newSize);
        if (replacement == nullptr)
            return nullptr;

        if (newSize >= accountedOld)
            budget->usedBytes += newSize - accountedOld;
        else
            budget->usedBytes -= std::min(
                accountedOld - newSize,
                budget->usedBytes);
        return replacement;
    }

    void MetadataInstructionHook(lua_State* state, lua_Debug*)
    {
        if (activeMetadataInstructionBudget == nullptr)
            return;
        if (*activeMetadataInstructionBudget <= InstructionHookQuantum)
        {
            *activeMetadataInstructionBudget = 0;
            luaL_error(
                state,
                "Renegade metadata instruction budget exceeded.");
            return;
        }
        *activeMetadataInstructionBudget -= InstructionHookQuantum;
    }

    bool ReadMetadataSource(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& source,
        std::string& error)
    {
        source.clear();
        if (!ValidateProjectScriptSource(
                projectRoot,
                projectRelativePath,
                error))
            return false;

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::u8path(projectRoot), ec);
        if (ec || root.empty())
        {
            error = "Could not resolve metadata project root: " + ec.message();
            return false;
        }

        const fs::path path = fs::weakly_canonical(
            root / fs::u8path(projectRelativePath), ec);
        if (ec || path.empty())
        {
            error = "Could not resolve metadata source: " + ec.message();
            return false;
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not open metadata source: " + projectRelativePath;
            return false;
        }
        source.assign(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
        if (stream.bad())
        {
            source.clear();
            error = "Could not read complete metadata source: " +
                projectRelativePath;
            return false;
        }
        if (source.find('\0') != std::string::npos)
        {
            source.clear();
            error = "Metadata source contains an embedded NUL byte.";
            return false;
        }
        error.clear();
        return true;
    }

    bool IsIdentifier(const std::string& value)
    {
        if (value.empty())
            return false;
        const auto isAlpha = [](const unsigned char c)
        {
            return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') || c == '_';
        };
        const auto isDigit = [](const unsigned char c)
        {
            return c >= '0' && c <= '9';
        };
        if (!isAlpha(static_cast<unsigned char>(value.front())))
            return false;
        return std::all_of(
            value.begin() + 1,
            value.end(),
            [&](const char c)
            {
                const auto valueByte = static_cast<unsigned char>(c);
                return isAlpha(valueByte) || isDigit(valueByte);
            });
    }

    bool ReadStringField(
        lua_State* state,
        const int tableIndex,
        const char* key,
        std::string& value,
        const bool required,
        MetadataCaptureContext& context,
        const std::string& fieldPath,
        const std::size_t maximumLength)
    {
        lua_getfield(state, tableIndex, key);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 1);
            if (required)
            {
                AddDiagnostic(
                    context,
                    "metadata.missing_field",
                    fieldPath,
                    "Required metadata field is missing.");
                return false;
            }
            value.clear();
            return true;
        }
        if (!lua_isstring(state, -1))
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.invalid_type",
                fieldPath,
                "Metadata field must be a string.");
            return false;
        }
        std::size_t length = 0;
        const char* text = lua_tolstring(state, -1, &length);
        value.assign(text == nullptr ? "" : text, length);
        lua_pop(state, 1);
        if (value.size() > maximumLength)
        {
            AddDiagnostic(
                context,
                "metadata.text_too_long",
                fieldPath,
                "Metadata text exceeds the supported length.");
            return false;
        }
        if (required && value.empty())
        {
            AddDiagnostic(
                context,
                "metadata.empty_field",
                fieldPath,
                "Required metadata text must not be empty.");
            return false;
        }
        return true;
    }

    bool ReadOptionalNumber(
        lua_State* state,
        const int tableIndex,
        const char* key,
        bool& present,
        double& value,
        MetadataCaptureContext& context,
        const std::string& fieldPath,
        const bool integerOnly)
    {
        lua_getfield(state, tableIndex, key);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 1);
            present = false;
            value = 0.0;
            return true;
        }
        if (!lua_isnumber(state, -1) ||
            (integerOnly && !lua_isinteger(state, -1)))
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.invalid_number",
                fieldPath,
                integerOnly
                    ? "Metadata field must be an integer."
                    : "Metadata field must be a finite number.");
            return false;
        }
        value = lua_tonumber(state, -1);
        lua_pop(state, 1);
        present = true;
        if (!std::isfinite(value))
        {
            AddDiagnostic(
                context,
                "metadata.non_finite_number",
                fieldPath,
                "Metadata numbers must be finite.");
            return false;
        }
        return true;
    }

    bool ParsePresentation(
        const std::string& token,
        ScriptPresentation& presentation)
    {
        if (token == "ACTION")
        {
            presentation = ScriptPresentation::Action;
            return true;
        }
        if (token == "SCRIPT")
        {
            presentation = ScriptPresentation::Script;
            return true;
        }
        if (token == "GLOBAL SCRIPT")
        {
            presentation = ScriptPresentation::GlobalScript;
            return true;
        }
        return false;
    }

    bool ParsePropertyType(
        const std::string& token,
        ScriptPropertyType& type)
    {
        if (token == "boolean") type = ScriptPropertyType::Boolean;
        else if (token == "integer") type = ScriptPropertyType::Integer;
        else if (token == "float") type = ScriptPropertyType::Float;
        else if (token == "string") type = ScriptPropertyType::String;
        else if (token == "colour") type = ScriptPropertyType::Colour;
        else if (token == "vector2") type = ScriptPropertyType::Vector2;
        else if (token == "vector3") type = ScriptPropertyType::Vector3;
        else if (token == "entity") type = ScriptPropertyType::EntityReference;
        else if (token == "asset") type = ScriptPropertyType::AssetReference;
        else if (token == "animation") type = ScriptPropertyType::Animation;
        else if (token == "audio") type = ScriptPropertyType::Audio;
        else if (token == "enum") type = ScriptPropertyType::Enum;
        else return false;
        return true;
    }

    bool ReadVectorComponent(
        lua_State* state,
        const int tableIndex,
        const char* key,
        float& value,
        MetadataCaptureContext& context,
        const std::string& fieldPath)
    {
        lua_getfield(state, tableIndex, key);
        if (!lua_isnumber(state, -1))
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.invalid_vector",
                fieldPath,
                "Vector/colour components must be finite numbers.");
            return false;
        }
        const double number = lua_tonumber(state, -1);
        lua_pop(state, 1);
        if (!std::isfinite(number))
        {
            AddDiagnostic(
                context,
                "metadata.invalid_vector",
                fieldPath,
                "Vector/colour components must be finite numbers.");
            return false;
        }
        value = static_cast<float>(number);
        return true;
    }

    bool ParseEnumOptions(
        lua_State* state,
        const int propertyIndex,
        ScriptMetadataPropertyDescriptor& property,
        MetadataCaptureContext& context,
        const std::string& propertyPath)
    {
        lua_getfield(state, propertyIndex, "options");
        if (!lua_istable(state, -1))
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.enum_options_required",
                propertyPath + ".options",
                "Enum properties require an ordered options array.");
            return false;
        }

        const int optionsIndex = lua_absindex(state, -1);
        const std::size_t count = lua_rawlen(state, optionsIndex);
        if (count == 0 || count > MaximumEnumOptions)
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.invalid_enum_options",
                propertyPath + ".options",
                "Enum options must contain between 1 and 256 entries.");
            return false;
        }

        std::unordered_set<std::string> values;
        bool valid = true;
        property.enumOptions.clear();
        property.enumOptions.reserve(count);
        for (std::size_t i = 1; i <= count; ++i)
        {
            lua_rawgeti(state, optionsIndex, static_cast<lua_Integer>(i));
            ScriptMetadataEnumOption option;
            const std::string optionPath = propertyPath + ".options[" +
                std::to_string(i) + "]";
            if (lua_isstring(state, -1))
            {
                std::size_t length = 0;
                const char* text = lua_tolstring(state, -1, &length);
                option.value.assign(text == nullptr ? "" : text, length);
                option.label = option.value;
            }
            else if (lua_istable(state, -1))
            {
                const int optionIndex = lua_absindex(state, -1);
                valid = ReadStringField(
                    state,
                    optionIndex,
                    "value",
                    option.value,
                    true,
                    context,
                    optionPath + ".value",
                    128) && valid;
                valid = ReadStringField(
                    state,
                    optionIndex,
                    "label",
                    option.label,
                    false,
                    context,
                    optionPath + ".label",
                    128) && valid;
                if (option.label.empty())
                    option.label = option.value;
            }
            else
            {
                AddDiagnostic(
                    context,
                    "metadata.invalid_enum_option",
                    optionPath,
                    "Enum options must be strings or { value, label } tables.");
                valid = false;
            }
            lua_pop(state, 1);

            if (!option.value.empty() && !values.insert(option.value).second)
            {
                AddDiagnostic(
                    context,
                    "metadata.duplicate_enum_option",
                    optionPath,
                    "Enum option values must be unique.");
                valid = false;
            }
            property.enumOptions.push_back(std::move(option));
        }
        lua_pop(state, 1);
        return valid;
    }

    bool ParseDefault(
        lua_State* state,
        const int propertyIndex,
        ScriptMetadataPropertyDescriptor& property,
        MetadataCaptureContext& context,
        const std::string& propertyPath)
    {
        property.defaultValue = {};
        property.defaultValue.name = property.name;
        property.defaultValue.type = property.type;
        property.hasDefault = true;

        lua_getfield(state, propertyIndex, "default");
        const bool missing = lua_isnil(state, -1);

        using Type = ScriptPropertyType;
        switch (property.type)
        {
        case Type::EntityReference:
        case Type::AssetReference:
        case Type::Animation:
        case Type::Audio:
            if (!missing)
            {
                AddDiagnostic(
                    context,
                    "metadata.reference_default_forbidden",
                    propertyPath + ".default",
                    "Reference properties cannot embed source-authored IDs; omit default or use nil.");
                lua_pop(state, 1);
                return false;
            }
            lua_pop(state, 1);
            return true;
        default:
            break;
        }

        if (missing)
        {
            lua_pop(state, 1);
            property.hasDefault = false;
            AddDiagnostic(
                context,
                "metadata.default_required",
                propertyPath + ".default",
                "Non-reference properties require an explicit default value.");
            return false;
        }

        bool valid = true;
        switch (property.type)
        {
        case Type::Boolean:
            if (!lua_isboolean(state, -1)) valid = false;
            else property.defaultValue.booleanValue = lua_toboolean(state, -1) != 0;
            break;
        case Type::Integer:
            if (!lua_isinteger(state, -1)) valid = false;
            else property.defaultValue.integerValue =
                static_cast<std::int64_t>(lua_tointeger(state, -1));
            break;
        case Type::Float:
            if (!lua_isnumber(state, -1) ||
                !std::isfinite(lua_tonumber(state, -1))) valid = false;
            else property.defaultValue.numberValue =
                static_cast<float>(lua_tonumber(state, -1));
            break;
        case Type::String:
        case Type::Enum:
            if (!lua_isstring(state, -1)) valid = false;
            else
            {
                std::size_t length = 0;
                const char* text = lua_tolstring(state, -1, &length);
                property.defaultValue.textValue.assign(
                    text == nullptr ? "" : text, length);
            }
            break;
        case Type::Colour:
        case Type::Vector2:
        case Type::Vector3:
            if (!lua_istable(state, -1))
            {
                valid = false;
                break;
            }
            else
            {
                const int valueIndex = lua_absindex(state, -1);
                if (property.type == Type::Colour)
                {
                    valid = ReadVectorComponent(state, valueIndex, "r",
                        property.defaultValue.x, context,
                        propertyPath + ".default.r") && valid;
                    valid = ReadVectorComponent(state, valueIndex, "g",
                        property.defaultValue.y, context,
                        propertyPath + ".default.g") && valid;
                    valid = ReadVectorComponent(state, valueIndex, "b",
                        property.defaultValue.z, context,
                        propertyPath + ".default.b") && valid;
                    valid = ReadVectorComponent(state, valueIndex, "a",
                        property.defaultValue.w, context,
                        propertyPath + ".default.a") && valid;
                    if (valid &&
                        (property.defaultValue.x < 0.0f || property.defaultValue.x > 1.0f ||
                         property.defaultValue.y < 0.0f || property.defaultValue.y > 1.0f ||
                         property.defaultValue.z < 0.0f || property.defaultValue.z > 1.0f ||
                         property.defaultValue.w < 0.0f || property.defaultValue.w > 1.0f))
                    {
                        AddDiagnostic(
                            context,
                            "metadata.colour_out_of_range",
                            propertyPath + ".default",
                            "Colour defaults use normalized RGBA components in the range 0..1.");
                        valid = false;
                    }
                }
                else
                {
                    valid = ReadVectorComponent(state, valueIndex, "x",
                        property.defaultValue.x, context,
                        propertyPath + ".default.x") && valid;
                    valid = ReadVectorComponent(state, valueIndex, "y",
                        property.defaultValue.y, context,
                        propertyPath + ".default.y") && valid;
                    if (property.type == Type::Vector3)
                        valid = ReadVectorComponent(state, valueIndex, "z",
                            property.defaultValue.z, context,
                            propertyPath + ".default.z") && valid;
                }
            }
            break;
        case Type::EntityReference:
        case Type::AssetReference:
        case Type::Animation:
        case Type::Audio:
            break;
        }
        lua_pop(state, 1);

        if (!valid)
        {
            AddDiagnostic(
                context,
                "metadata.invalid_default",
                propertyPath + ".default",
                "Default value does not match the declared property type.");
        }
        return valid;
    }

    bool ParseProperty(
        lua_State* state,
        const int propertyIndex,
        const std::size_t ordinal,
        ScriptMetadataPropertyDescriptor& property,
        MetadataCaptureContext& context)
    {
        const std::string path = "properties[" + std::to_string(ordinal) + "]";
        bool valid = true;

        valid = ReadStringField(
            state, propertyIndex, "name", property.name, true,
            context, path + ".name", 64) && valid;
        if (!property.name.empty() && !IsIdentifier(property.name))
        {
            AddDiagnostic(
                context,
                "metadata.invalid_property_name",
                path + ".name",
                "Property names must be Lua-style ASCII identifiers.");
            valid = false;
        }

        valid = ReadStringField(
            state, propertyIndex, "label", property.label, false,
            context, path + ".label", 128) && valid;
        if (property.label.empty())
            property.label = property.name;
        valid = ReadStringField(
            state, propertyIndex, "description", property.description, false,
            context, path + ".description", 1024) && valid;

        std::string typeToken;
        valid = ReadStringField(
            state, propertyIndex, "type", typeToken, true,
            context, path + ".type", 32) && valid;
        if (!typeToken.empty() && !ParsePropertyType(typeToken, property.type))
        {
            AddDiagnostic(
                context,
                "metadata.unknown_property_type",
                path + ".type",
                "Unknown property type. Supported: boolean, integer, float, string, colour, vector2, vector3, entity, asset, animation, audio, enum.");
            valid = false;
        }

        if (!typeToken.empty() && ParsePropertyType(typeToken, property.type))
        {
            valid = ParseDefault(
                state, propertyIndex, property, context, path) && valid;

            const bool numeric =
                property.type == ScriptPropertyType::Integer ||
                property.type == ScriptPropertyType::Float;
            if (numeric)
            {
                const bool integerOnly =
                    property.type == ScriptPropertyType::Integer;
                valid = ReadOptionalNumber(
                    state, propertyIndex, "min", property.hasMinimum,
                    property.minimum, context, path + ".min", integerOnly) && valid;
                valid = ReadOptionalNumber(
                    state, propertyIndex, "max", property.hasMaximum,
                    property.maximum, context, path + ".max", integerOnly) && valid;
                valid = ReadOptionalNumber(
                    state, propertyIndex, "step", property.hasStep,
                    property.step, context, path + ".step", integerOnly) && valid;
                if (property.hasMinimum && property.hasMaximum &&
                    property.minimum > property.maximum)
                {
                    AddDiagnostic(
                        context,
                        "metadata.invalid_range",
                        path,
                        "Property min must not exceed max.");
                    valid = false;
                }
                if (property.hasStep && property.step <= 0.0)
                {
                    AddDiagnostic(
                        context,
                        "metadata.invalid_step",
                        path + ".step",
                        "Property step must be greater than zero.");
                    valid = false;
                }
                if (property.hasDefault)
                {
                    const double defaultNumber =
                        property.type == ScriptPropertyType::Integer
                            ? static_cast<double>(property.defaultValue.integerValue)
                            : static_cast<double>(property.defaultValue.numberValue);
                    if ((property.hasMinimum && defaultNumber < property.minimum) ||
                        (property.hasMaximum && defaultNumber > property.maximum))
                    {
                        AddDiagnostic(
                            context,
                            "metadata.default_out_of_range",
                            path + ".default",
                            "Default value must lie inside the declared numeric range.");
                        valid = false;
                    }
                }
            }
            if (property.type == ScriptPropertyType::Enum)
            {
                valid = ParseEnumOptions(
                    state, propertyIndex, property, context, path) && valid;
                if (property.hasDefault && !property.defaultValue.textValue.empty())
                {
                    const bool found = std::any_of(
                        property.enumOptions.begin(),
                        property.enumOptions.end(),
                        [&](const ScriptMetadataEnumOption& option)
                        {
                            return option.value == property.defaultValue.textValue;
                        });
                    if (!found)
                    {
                        AddDiagnostic(
                            context,
                            "metadata.enum_default_missing",
                            path + ".default",
                            "Enum default must match one declared option value.");
                        valid = false;
                    }
                }
            }
        }
        return valid;
    }

    bool ParseMetadataTable(
        lua_State* state,
        const int tableIndex,
        MetadataCaptureContext& context)
    {
        if (context.result == nullptr)
            return false;
        auto& descriptor = context.result->descriptor;
        descriptor = {};
        bool valid = true;

        lua_getfield(state, tableIndex, "schema_version");
        if (!lua_isinteger(state, -1))
        {
            AddDiagnostic(
                context,
                "metadata.schema_version_required",
                "schema_version",
                "Metadata schema_version must be integer 1.");
            valid = false;
        }
        else
        {
            const lua_Integer version = lua_tointeger(state, -1);
            if (version != static_cast<lua_Integer>(ScriptMetadataSchemaVersion))
            {
                AddDiagnostic(
                    context,
                    "metadata.unsupported_schema",
                    "schema_version",
                    "Unsupported metadata schema version.");
                valid = false;
            }
            else
            {
                descriptor.schemaVersion = ScriptMetadataSchemaVersion;
            }
        }
        lua_pop(state, 1);

        valid = ReadStringField(
            state, tableIndex, "name", descriptor.name, true,
            context, "name", 128) && valid;
        valid = ReadStringField(
            state, tableIndex, "description", descriptor.description, false,
            context, "description", 2048) && valid;
        valid = ReadStringField(
            state, tableIndex, "category", descriptor.category, true,
            context, "category", 128) && valid;

        std::string role;
        valid = ReadStringField(
            state, tableIndex, "role", role, true,
            context, "role", 32) && valid;
        if (!role.empty() && !ParsePresentation(role, descriptor.presentation))
        {
            AddDiagnostic(
                context,
                "metadata.invalid_role",
                "role",
                "Role must be exactly ACTION, SCRIPT, or GLOBAL SCRIPT.");
            valid = false;
        }

        lua_getfield(state, tableIndex, "properties");
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 1);
            descriptor.properties.clear();
            return valid;
        }
        if (!lua_istable(state, -1))
        {
            lua_pop(state, 1);
            AddDiagnostic(
                context,
                "metadata.invalid_properties",
                "properties",
                "Properties must be an ordered array when present.");
            return false;
        }

        const int propertiesIndex = lua_absindex(state, -1);
        const std::size_t count = lua_rawlen(state, propertiesIndex);
        if (count > MaximumMetadataProperties)
        {
            AddDiagnostic(
                context,
                "metadata.too_many_properties",
                "properties",
                "Metadata supports at most 256 properties per source.");
            valid = false;
        }

        std::unordered_set<std::string> names;
        descriptor.properties.clear();
        descriptor.properties.reserve(std::min(count, MaximumMetadataProperties));
        for (std::size_t i = 1;
             i <= count && i <= MaximumMetadataProperties;
             ++i)
        {
            lua_rawgeti(state, propertiesIndex, static_cast<lua_Integer>(i));
            if (!lua_istable(state, -1))
            {
                AddDiagnostic(
                    context,
                    "metadata.invalid_property",
                    "properties[" + std::to_string(i) + "]",
                    "Each property entry must be a table.");
                valid = false;
                lua_pop(state, 1);
                continue;
            }
            ScriptMetadataPropertyDescriptor property;
            const int propertyIndex = lua_absindex(state, -1);
            valid = ParseProperty(
                state, propertyIndex, i, property, context) && valid;
            lua_pop(state, 1);

            if (!property.name.empty() && !names.insert(property.name).second)
            {
                AddDiagnostic(
                    context,
                    "metadata.duplicate_property",
                    "properties[" + std::to_string(i) + "].name",
                    "Property names must be unique within one source.");
                valid = false;
            }
            descriptor.properties.push_back(std::move(property));
        }
        lua_pop(state, 1);
        return valid;
    }

    static int CaptureMetadataLua(lua_State* state)
    {
        auto* context = static_cast<MetadataCaptureContext*>(
            lua_touserdata(state, lua_upvalueindex(1)));
        if (context == nullptr || context->result == nullptr)
        {
            lua_pushliteral(state, "Renegade metadata capture lost its context.");
            return lua_error(state);
        }

        context->captured = true;
        {
            if (!lua_istable(state, 1))
            {
                AddDiagnostic(
                    *context,
                    "metadata.declaration_not_table",
                    "metadata",
                    "renegade.metadata expects one metadata table.");
            }
            else
            {
                (void)ParseMetadataTable(
                    state,
                    lua_absindex(state, 1),
                    *context);
            }
        }

        // No non-trivial C++ locals are live across lua_error(). The sentinel
        // intentionally terminates the metadata-only execution immediately so
        // gameplay setup below renegade.metadata() is never run by Studio.
        lua_pushliteral(state, MetadataCapturedSentinel);
        return lua_error(state);
    }

    void RemoveGlobal(lua_State* state, const char* name)
    {
        lua_pushnil(state);
        lua_setglobal(state, name);
    }

    bool InstallMetadataEnvironment(
        lua_State* state,
        MetadataCaptureContext& context)
    {
        luaL_requiref(state, "_G", luaopen_base, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(state, 1);
        luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
        lua_pop(state, 1);

        RemoveGlobal(state, "dofile");
        RemoveGlobal(state, "loadfile");
        RemoveGlobal(state, "load");
        RemoveGlobal(state, "collectgarbage");
        RemoveGlobal(state, "package");
        RemoveGlobal(state, "io");
        RemoveGlobal(state, "os");
        RemoveGlobal(state, "debug");
        RemoveGlobal(state, "coroutine");
        RemoveGlobal(state, "require");

        lua_newtable(state);
        lua_pushlightuserdata(state, &context);
        lua_pushcclosure(state, CaptureMetadataLua, 1);
        lua_setfield(state, -2, "metadata");
        lua_setglobal(state, "renegade");
        return true;
    }
}

namespace renegade::bridge
{
    ScriptMetadataEvaluationResult EvaluateScriptMetadata(
        const std::string& projectRoot,
        const std::string& projectRelativeSourcePath,
        const std::size_t memoryBudgetBytes,
        const std::size_t instructionBudget)
    {
        ScriptMetadataEvaluationResult result;
        MetadataCaptureContext context;
        context.result = &result;
        context.sourcePath = projectRelativeSourcePath;

        if (memoryBudgetBytes < 64u * 1024u)
        {
            AddDiagnostic(
                context,
                "metadata.invalid_memory_budget",
                "",
                "Metadata memory budget is too small.");
            return result;
        }
        if (instructionBudget < InstructionHookQuantum)
        {
            AddDiagnostic(
                context,
                "metadata.invalid_instruction_budget",
                "",
                "Metadata instruction budget is too small.");
            return result;
        }

        std::string source;
        std::string sourceError;
        if (!ReadMetadataSource(
                projectRoot,
                projectRelativeSourcePath,
                source,
                sourceError))
        {
            AddDiagnostic(
                context,
                "metadata.source_invalid",
                "",
                std::move(sourceError));
            return result;
        }

        MetadataMemoryBudget memory;
        memory.limitBytes = memoryBudgetBytes;
        lua_State* state = lua_newstate(MetadataAllocate, &memory);
        if (state == nullptr)
        {
            AddDiagnostic(
                context,
                "metadata.state_allocation_failed",
                "",
                "Could not create the restricted metadata Lua state.");
            return result;
        }

        InstallMetadataEnvironment(state, context);
        if (luaL_loadbufferx(
                state,
                source.data(),
                source.size(),
                projectRelativeSourcePath.c_str(),
                "t") != LUA_OK)
        {
            const char* message = lua_tostring(state, -1);
            AddDiagnostic(
                context,
                "metadata.lua_syntax_error",
                "",
                message == nullptr
                    ? "Lua metadata source did not compile."
                    : message);
            lua_close(state);
            return result;
        }

        std::size_t remaining =
            std::max(instructionBudget, InstructionHookQuantum);
        activeMetadataInstructionBudget = &remaining;
        lua_sethook(
            state,
            MetadataInstructionHook,
            LUA_MASKCOUNT,
            static_cast<int>(InstructionHookQuantum));
        const int status = lua_pcall(state, 0, 0, 0);
        lua_sethook(state, nullptr, 0, 0);
        activeMetadataInstructionBudget = nullptr;

        if (!context.captured)
        {
            const char* message = status == LUA_OK
                ? nullptr
                : lua_tostring(state, -1);
            AddDiagnostic(
                context,
                status == LUA_OK
                    ? "metadata.declaration_missing"
                    : "metadata.evaluation_failed",
                "",
                status == LUA_OK
                    ? "Script source must declare renegade.metadata({...}) before gameplay setup."
                    : (message == nullptr
                        ? "Metadata evaluation failed before declaration."
                        : message));
        }

        result.succeeded = context.captured && !HasErrors(result);
        lua_close(state);
        return result;
    }

    bool ApplyScriptMetadataDefaults(
        const ScriptMetadataDescriptor& metadata,
        ScriptAttachment& attachment,
        std::string& error)
    {
        if (metadata.schemaVersion != ScriptMetadataSchemaVersion)
        {
            error = "Unsupported script metadata schema version.";
            return false;
        }
        if (attachment.presentation != metadata.presentation)
        {
            error = "Script attachment presentation does not match source metadata role.";
            return false;
        }
        if (metadata.presentation == ScriptPresentation::GlobalScript)
        {
            if (attachment.scope != ScriptScope::Level)
            {
                error = "GLOBAL SCRIPT metadata can only seed a Level attachment.";
                return false;
            }
        }
        else if (attachment.scope != ScriptScope::Entity)
        {
            error = "ACTION/SCRIPT metadata can only seed an Entity attachment.";
            return false;
        }

        for (const auto& descriptor : metadata.properties)
        {
            const auto found = std::find_if(
                attachment.properties.begin(),
                attachment.properties.end(),
                [&](const ScriptPropertyValue& value)
                {
                    return value.name == descriptor.name;
                });
            if (found != attachment.properties.end())
            {
                if (found->type != descriptor.type)
                {
                    error = "Persisted property type does not match metadata for '" +
                        descriptor.name + "'.";
                    return false;
                }
                continue;
            }
            if (descriptor.hasDefault)
                attachment.properties.push_back(descriptor.defaultValue);
        }
        error.clear();
        return true;
    }
}
