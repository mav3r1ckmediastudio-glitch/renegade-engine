#include "renegade/bridge/ScriptDocumentService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

#include <wiConfig.h>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ScriptAttachment;
    using renegade::bridge::ScriptDependency;
    using renegade::bridge::ScriptDependencyKind;
    using renegade::bridge::ScriptDocument;
    using renegade::bridge::ScriptPresentation;
    using renegade::bridge::ScriptPropertyType;
    using renegade::bridge::ScriptPropertyValue;
    using renegade::bridge::ScriptProvenance;
    using renegade::bridge::ScriptProvenanceKind;
    using renegade::bridge::ScriptScope;
    using renegade::bridge::StableId;

    constexpr std::size_t MaximumAttachmentCount = 65535;
    constexpr std::size_t MaximumNestedCount = 65535;

    std::string LowerAscii(std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return character >= 'A' && character <= 'Z'
                    ? static_cast<char>(character - 'A' + 'a')
                    : static_cast<char>(character);
            });
        return value;
    }

    bool IsSafeRelativePath(const std::string& value)
    {
        if (value.empty())
            return false;
        const fs::path path = fs::u8path(value);
        if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
            return false;
        return std::none_of(
            path.begin(), path.end(),
            [](const fs::path& part) { return part == ".."; });
    }

    bool IsContentRelativePath(const std::string& value)
    {
        if (!IsSafeRelativePath(value))
            return false;
        const fs::path path = fs::u8path(value).lexically_normal();
        auto item = path.begin();
        return item != path.end() && LowerAscii(item->generic_u8string()) == "content";
    }

    bool IsScriptSourcePath(const std::string& value)
    {
        if (!IsSafeRelativePath(value))
            return false;
        const fs::path path = fs::u8path(value).lexically_normal();
        auto item = path.begin();
        if (item == path.end() || LowerAscii(item->generic_u8string()) != "content")
            return false;
        ++item;
        if (item == path.end() || LowerAscii(item->generic_u8string()) != "scripts")
            return false;
        return LowerAscii(path.extension().generic_u8string()) == ".lua";
    }

    bool IsValidPropertyName(const std::string& value)
    {
        if (value.empty())
            return false;
        const auto first = static_cast<unsigned char>(value.front());
        if (!((first >= 'a' && first <= 'z') ||
              (first >= 'A' && first <= 'Z') || first == '_'))
            return false;
        return std::all_of(
            value.begin() + 1, value.end(),
            [](const unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') ||
                    character == '_' || character == '.' || character == '-';
            });
    }

    bool IsValidCapability(const std::string& value)
    {
        if (value.empty())
            return false;
        return std::all_of(
            value.begin(), value.end(),
            [](const unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= '0' && character <= '9') ||
                    character == '.' || character == '_' || character == '-';
            });
    }

    std::string IndexedSection(const std::string& prefix, const std::size_t index)
    {
        return prefix + std::to_string(index);
    }

    std::string HexEncode(const std::string& value)
    {
        static constexpr char Hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(value.size() * 2);
        for (const unsigned char byte : value)
        {
            result.push_back(Hex[(byte >> 4) & 0x0F]);
            result.push_back(Hex[byte & 0x0F]);
        }
        return result;
    }

    int HexNibble(const char character) noexcept
    {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    }

    bool HexDecode(const std::string& value, std::string& result)
    {
        if ((value.size() % 2) != 0)
            return false;
        std::string decoded;
        decoded.reserve(value.size() / 2);
        for (std::size_t index = 0; index < value.size(); index += 2)
        {
            const int high = HexNibble(value[index]);
            const int low = HexNibble(value[index + 1]);
            if (high < 0 || low < 0)
                return false;
            decoded.push_back(static_cast<char>((high << 4) | low));
        }
        result = std::move(decoded);
        return true;
    }

    bool ReadHexText(
        const wi::config::Section& section,
        const char* key,
        std::string& value,
        std::string& error)
    {
        if (!section.Has(key) || !HexDecode(section.GetText(key), value))
        {
            error = std::string("Missing or malformed encoded field: ") + key;
            return false;
        }
        return true;
    }

    bool ParseUnsigned(
        const wi::config::Section& section,
        const char* key,
        std::uint32_t& value,
        std::string& error)
    {
        if (!section.Has(key))
        {
            error = std::string("Missing unsigned field: ") + key;
            return false;
        }
        try
        {
            std::size_t consumed = 0;
            const unsigned long parsed = std::stoul(section.GetText(key), &consumed, 10);
            if (consumed != section.GetText(key).size() ||
                parsed > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::out_of_range("uint32");
            }
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }
        catch (const std::exception&)
        {
            error = std::string("Malformed unsigned field: ") + key;
            return false;
        }
    }

    bool ParseCount(
        const wi::config::Section& section,
        const char* key,
        std::size_t maximum,
        std::size_t& value,
        std::string& error)
    {
        std::uint32_t parsed = 0;
        if (!ParseUnsigned(section, key, parsed, error))
            return false;
        if (static_cast<std::size_t>(parsed) > maximum)
        {
            error = std::string("Count exceeds S2 safety limit: ") + key;
            return false;
        }
        value = static_cast<std::size_t>(parsed);
        return true;
    }

    bool ParseInt64(
        const wi::config::Section& section,
        const char* key,
        std::int64_t& value,
        std::string& error)
    {
        if (!section.Has(key))
        {
            error = std::string("Missing integer field: ") + key;
            return false;
        }
        try
        {
            std::size_t consumed = 0;
            const std::string text = section.GetText(key);
            value = std::stoll(text, &consumed, 10);
            if (consumed != text.size())
                throw std::invalid_argument("integer");
            return true;
        }
        catch (const std::exception&)
        {
            error = std::string("Malformed integer field: ") + key;
            return false;
        }
    }

    bool ParseFloat(
        const wi::config::Section& section,
        const char* key,
        float& value,
        std::string& error)
    {
        if (!section.Has(key))
        {
            error = std::string("Missing float field: ") + key;
            return false;
        }
        try
        {
            std::size_t consumed = 0;
            const std::string text = section.GetText(key);
            value = std::stof(text, &consumed);
            if (consumed != text.size() || !std::isfinite(value))
                throw std::invalid_argument("float");
            return true;
        }
        catch (const std::exception&)
        {
            error = std::string("Malformed float field: ") + key;
            return false;
        }
    }

    bool ParseBool(
        const wi::config::Section& section,
        const char* key,
        bool& value,
        std::string& error)
    {
        if (!section.Has(key))
        {
            error = std::string("Missing boolean field: ") + key;
            return false;
        }
        const std::string text = LowerAscii(section.GetText(key));
        if (text == "true" || text == "1")
        {
            value = true;
            return true;
        }
        if (text == "false" || text == "0")
        {
            value = false;
            return true;
        }
        error = std::string("Malformed boolean field: ") + key;
        return false;
    }

    template<typename Enum>
    bool ParseEnumText(
        const std::string& text,
        Enum& value);

    template<>
    bool ParseEnumText(const std::string& text, ScriptScope& value)
    {
        if (text == "entity") value = ScriptScope::Entity;
        else if (text == "level") value = ScriptScope::Level;
        else if (text == "game") value = ScriptScope::Game;
        else return false;
        return true;
    }

    template<>
    bool ParseEnumText(const std::string& text, ScriptPresentation& value)
    {
        if (text == "action") value = ScriptPresentation::Action;
        else if (text == "script") value = ScriptPresentation::Script;
        else if (text == "global_script") value = ScriptPresentation::GlobalScript;
        else return false;
        return true;
    }

    template<>
    bool ParseEnumText(const std::string& text, ScriptPropertyType& value)
    {
        if (text == "boolean") value = ScriptPropertyType::Boolean;
        else if (text == "integer") value = ScriptPropertyType::Integer;
        else if (text == "float") value = ScriptPropertyType::Float;
        else if (text == "string") value = ScriptPropertyType::String;
        else if (text == "colour") value = ScriptPropertyType::Colour;
        else if (text == "vector2") value = ScriptPropertyType::Vector2;
        else if (text == "vector3") value = ScriptPropertyType::Vector3;
        else if (text == "entity_ref") value = ScriptPropertyType::EntityReference;
        else if (text == "asset_ref") value = ScriptPropertyType::AssetReference;
        else if (text == "animation") value = ScriptPropertyType::Animation;
        else if (text == "audio") value = ScriptPropertyType::Audio;
        else if (text == "enum") value = ScriptPropertyType::Enum;
        else return false;
        return true;
    }

    template<>
    bool ParseEnumText(const std::string& text, ScriptDependencyKind& value)
    {
        if (text == "script_module") value = ScriptDependencyKind::ScriptModule;
        else if (text == "asset") value = ScriptDependencyKind::Asset;
        else return false;
        return true;
    }

    template<>
    bool ParseEnumText(const std::string& text, ScriptProvenanceKind& value)
    {
        if (text == "project") value = ScriptProvenanceKind::Project;
        else if (text == "installed_library") value = ScriptProvenanceKind::InstalledLibrary;
        else if (text == "personal_library") value = ScriptProvenanceKind::PersonalLibrary;
        else return false;
        return true;
    }

    template<typename Enum>
    bool ParseEnum(
        const wi::config::Section& section,
        const char* key,
        Enum& value,
        std::string& error)
    {
        if (!section.Has(key) || !ParseEnumText(section.GetText(key), value))
        {
            error = std::string("Missing or unsupported enum field: ") + key;
            return false;
        }
        return true;
    }

    std::unordered_map<std::string, std::string> CaptureUnknownFields(
        const wi::config::Section& section,
        const std::unordered_set<std::string>& known)
    {
        std::unordered_map<std::string, std::string> result;
        for (const auto& item : section)
        {
            if (known.count(item.first) == 0)
                result.emplace(item.first, item.second);
        }
        return result;
    }

    void ApplyUnknownFields(
        wi::config::Section& section,
        const std::unordered_map<std::string, std::string>& fields,
        const std::unordered_set<std::string>& reserved)
    {
        std::vector<std::pair<std::string, std::string>> sorted;
        sorted.reserve(fields.size());
        for (const auto& item : fields)
        {
            if (reserved.count(item.first) == 0)
                sorted.push_back(item);
        }
        std::sort(sorted.begin(), sorted.end());
        for (const auto& item : sorted)
            section.Set(item.first.c_str(), item.second);
    }

    const std::unordered_set<std::string>& EnvelopeKeys()
    {
        static const std::unordered_set<std::string> keys = {
            "id", "project_id", "type", "path_hint", "generator", "migrated_from"
        };
        return keys;
    }

    const std::unordered_set<std::string>& ScriptDocumentKeys()
    {
        static const std::unordered_set<std::string> keys = {
            "format", "schema_version", "scene_document_id", "scene_path_hex",
            "attachment_count"
        };
        return keys;
    }

    const std::unordered_set<std::string>& AttachmentKeys()
    {
        static const std::unordered_set<std::string> keys = {
            "instance_id", "scope", "owner_entity_id", "source_id",
            "source_path_hex", "presentation", "enabled", "order",
            "api_version", "unsafe", "provenance_kind",
            "provenance_library_id_hex", "provenance_library_version_hex",
            "provenance_content_hash_hex", "dependency_count", "property_count",
            "capability_count"
        };
        return keys;
    }

    const std::unordered_set<std::string>& DependencyKeys()
    {
        static const std::unordered_set<std::string> keys = {
            "kind", "id", "path_hint_hex", "optional"
        };
        return keys;
    }

    const std::unordered_set<std::string>& PropertyKeys()
    {
        static const std::unordered_set<std::string> keys = {
            "name_hex", "type", "boolean", "integer", "number",
            "x", "y", "z", "w", "text_hex", "reference_id", "path_hint_hex"
        };
        return keys;
    }

    const std::unordered_set<std::string>& CapabilityKeys()
    {
        static const std::unordered_set<std::string> keys = { "value_hex" };
        return keys;
    }

    std::string AttachmentSection(const std::size_t index)
    {
        return IndexedSection("attachment_", index);
    }

    std::string DependencySection(const std::size_t attachment, const std::size_t dependency)
    {
        return AttachmentSection(attachment) + "_dependency_" + std::to_string(dependency);
    }

    std::string PropertySection(const std::size_t attachment, const std::size_t property)
    {
        return AttachmentSection(attachment) + "_property_" + std::to_string(property);
    }

    std::string CapabilitySection(const std::size_t attachment, const std::size_t capability)
    {
        return AttachmentSection(attachment) + "_capability_" + std::to_string(capability);
    }

    bool SameProvenance(const ScriptProvenance& left, const ScriptProvenance& right)
    {
        return left.kind == right.kind &&
            left.libraryId == right.libraryId &&
            left.libraryVersion == right.libraryVersion &&
            left.contentHash == right.contentHash;
    }

    bool SameDependencies(
        const std::vector<ScriptDependency>& left,
        const std::vector<ScriptDependency>& right)
    {
        if (left.size() != right.size())
            return false;
        const auto signature = [](const ScriptDependency& dependency)
        {
            return std::string(
                dependency.kind == ScriptDependencyKind::ScriptModule ? "0|" : "1|") +
                dependency.id + "|" + dependency.pathHint + "|" +
                (dependency.optional ? "1" : "0");
        };
        std::vector<std::string> a;
        std::vector<std::string> b;
        a.reserve(left.size());
        b.reserve(right.size());
        for (const auto& dependency : left) a.push_back(signature(dependency));
        for (const auto& dependency : right) b.push_back(signature(dependency));
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        return a == b;
    }

    bool SameCapabilities(
        const std::vector<std::string>& left,
        const std::vector<std::string>& right)
    {
        if (left.size() != right.size())
            return false;
        auto a = left;
        auto b = right;
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        return a == b;
    }

    bool SameSourceAuthority(const ScriptAttachment& left, const ScriptAttachment& right)
    {
        return left.sourceId == right.sourceId &&
            left.sourcePath == right.sourcePath &&
            left.presentation == right.presentation &&
            left.apiVersion == right.apiVersion &&
            left.unsafe == right.unsafe &&
            SameProvenance(left.provenance, right.provenance) &&
            SameDependencies(left.dependencies, right.dependencies) &&
            SameCapabilities(left.capabilities, right.capabilities);
    }

    bool ValidateProvenance(const ScriptProvenance& provenance, std::string& error)
    {
        if (provenance.kind != ScriptProvenanceKind::Project &&
            (provenance.libraryId.empty() || provenance.libraryVersion.empty()))
        {
            error = "Library-adopted script provenance requires library ID and version.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ValidateDependency(
        const ScriptDependency& dependency,
        const StableId& sourceId,
        std::string& error)
    {
        if (!renegade::bridge::IsValidStableId(dependency.id))
        {
            error = "A script dependency is missing a valid stable ID.";
            return false;
        }
        if (dependency.id == sourceId &&
            dependency.kind == ScriptDependencyKind::ScriptModule)
        {
            error = "A script source cannot declare itself as a module dependency.";
            return false;
        }
        if (!dependency.pathHint.empty())
        {
            if (dependency.kind == ScriptDependencyKind::ScriptModule)
            {
                if (!IsScriptSourcePath(dependency.pathHint))
                {
                    error = "Script-module dependency path must remain under Content/Scripts and end in .lua.";
                    return false;
                }
            }
            else if (!IsContentRelativePath(dependency.pathHint))
            {
                error = "Asset dependency path hint must remain under Content/.";
                return false;
            }
        }
        error.clear();
        return true;
    }

    bool ValidateProperty(const ScriptPropertyValue& property, std::string& error)
    {
        if (!IsValidPropertyName(property.name))
        {
            error = "Script property names must be stable metadata symbols.";
            return false;
        }

        const auto finite = [](const float value) { return std::isfinite(value); };
        switch (property.type)
        {
        case ScriptPropertyType::Float:
            if (!finite(property.numberValue))
            {
                error = "Script float property is not finite.";
                return false;
            }
            break;
        case ScriptPropertyType::Colour:
            if (!finite(property.x) || !finite(property.y) || !finite(property.z) ||
                !finite(property.w) || property.x < 0.0f || property.x > 1.0f ||
                property.y < 0.0f || property.y > 1.0f ||
                property.z < 0.0f || property.z > 1.0f ||
                property.w < 0.0f || property.w > 1.0f)
            {
                error = "Script colour property must contain finite 0..1 RGBA values.";
                return false;
            }
            break;
        case ScriptPropertyType::Vector2:
            if (!finite(property.x) || !finite(property.y))
            {
                error = "Script Vector2 property is not finite.";
                return false;
            }
            break;
        case ScriptPropertyType::Vector3:
            if (!finite(property.x) || !finite(property.y) || !finite(property.z))
            {
                error = "Script Vector3 property is not finite.";
                return false;
            }
            break;
        case ScriptPropertyType::EntityReference:
        case ScriptPropertyType::AssetReference:
        case ScriptPropertyType::Animation:
        case ScriptPropertyType::Audio:
            if (!property.referenceId.empty() &&
                !renegade::bridge::IsValidStableId(property.referenceId))
            {
                error = "Script reference property contains a malformed stable ID.";
                return false;
            }
            if (!property.pathHint.empty() && !IsSafeRelativePath(property.pathHint))
            {
                error = "Script reference path hint must remain project-relative.";
                return false;
            }
            break;
        case ScriptPropertyType::Enum:
            if (property.textValue.empty())
            {
                error = "Script enum property requires a value.";
                return false;
            }
            break;
        case ScriptPropertyType::Boolean:
        case ScriptPropertyType::Integer:
        case ScriptPropertyType::String:
            break;
        }
        error.clear();
        return true;
    }

    bool ValidateAttachment(const ScriptAttachment& attachment, std::string& error)
    {
        if (!renegade::bridge::IsValidStableId(attachment.scriptInstanceId))
        {
            error = "Script attachment is missing a valid ScriptInstanceId.";
            return false;
        }
        if (!renegade::bridge::IsValidStableId(attachment.sourceId))
        {
            error = "Script attachment is missing a valid sourceId.";
            return false;
        }
        if (!IsScriptSourcePath(attachment.sourcePath))
        {
            error = "Script sourcePath must remain under Content/Scripts and end in .lua.";
            return false;
        }
        if (attachment.apiVersion == 0)
        {
            error = "Script apiVersion must be greater than zero.";
            return false;
        }
        if (attachment.scope == ScriptScope::Game)
        {
            error = "Game script scope is reserved but has no schema-v1 semantics.";
            return false;
        }
        if (attachment.scope == ScriptScope::Entity)
        {
            if (!renegade::bridge::IsValidStableId(attachment.ownerEntityId))
            {
                error = "Entity script attachment requires a valid ownerEntityId.";
                return false;
            }
            if (attachment.presentation == ScriptPresentation::GlobalScript)
            {
                error = "GLOBAL SCRIPT presentation is level-scoped, not entity-scoped.";
                return false;
            }
        }
        else
        {
            if (!attachment.ownerEntityId.empty())
            {
                error = "Level GLOBAL SCRIPT attachment must not carry ownerEntityId.";
                return false;
            }
            if (attachment.presentation != ScriptPresentation::GlobalScript)
            {
                error = "Level scope requires GLOBAL SCRIPT presentation.";
                return false;
            }
        }
        if (!ValidateProvenance(attachment.provenance, error))
            return false;

        std::unordered_set<std::string> dependencies;
        for (const auto& dependency : attachment.dependencies)
        {
            if (!ValidateDependency(dependency, attachment.sourceId, error))
                return false;
            const std::string key =
                std::to_string(static_cast<int>(dependency.kind)) + "|" + dependency.id;
            if (!dependencies.insert(key).second)
            {
                error = "Script source contains a duplicate governed dependency.";
                return false;
            }
        }

        std::unordered_set<std::string> capabilities;
        for (const auto& capability : attachment.capabilities)
        {
            if (!IsValidCapability(capability) || !capabilities.insert(capability).second)
            {
                error = "Script source contains an invalid or duplicate capability.";
                return false;
            }
        }

        std::unordered_set<std::string> properties;
        for (const auto& property : attachment.properties)
        {
            if (!ValidateProperty(property, error))
                return false;
            if (!properties.insert(property.name).second)
            {
                error = "Script attachment contains duplicate property state: " + property.name;
                return false;
            }
        }
        error.clear();
        return true;
    }

    std::string OrderGroup(const ScriptAttachment& attachment)
    {
        return std::to_string(static_cast<int>(attachment.scope)) + "|" +
            attachment.ownerEntityId;
    }

    void WriteProvenance(wi::config::Section& section, const ScriptProvenance& provenance)
    {
        section.Set("provenance_kind", renegade::bridge::ScriptProvenanceKindName(provenance.kind));
        section.Set("provenance_library_id_hex", HexEncode(provenance.libraryId));
        section.Set("provenance_library_version_hex", HexEncode(provenance.libraryVersion));
        section.Set("provenance_content_hash_hex", HexEncode(provenance.contentHash));
        ApplyUnknownFields(section, provenance.unknownFields, AttachmentKeys());
    }

    bool ReadProvenance(
        const wi::config::Section& section,
        ScriptProvenance& provenance,
        std::string& error)
    {
        if (!ParseEnum(section, "provenance_kind", provenance.kind, error) ||
            !ReadHexText(section, "provenance_library_id_hex", provenance.libraryId, error) ||
            !ReadHexText(section, "provenance_library_version_hex", provenance.libraryVersion, error) ||
            !ReadHexText(section, "provenance_content_hash_hex", provenance.contentHash, error))
        {
            return false;
        }
        return true;
    }

    void WriteProperty(wi::config::Section& section, const ScriptPropertyValue& property)
    {
        section.Set("name_hex", HexEncode(property.name));
        section.Set("type", renegade::bridge::ScriptPropertyTypeName(property.type));
        section.Set("boolean", property.booleanValue);
        section.Set("integer", std::to_string(property.integerValue));
        section.Set("number", property.numberValue);
        section.Set("x", property.x);
        section.Set("y", property.y);
        section.Set("z", property.z);
        section.Set("w", property.w);
        section.Set("text_hex", HexEncode(property.textValue));
        section.Set("reference_id", property.referenceId);
        section.Set("path_hint_hex", HexEncode(property.pathHint));
        ApplyUnknownFields(section, property.unknownFields, PropertyKeys());
    }

    bool ReadProperty(
        const wi::config::Section& section,
        ScriptPropertyValue& property,
        std::string& error)
    {
        if (!ReadHexText(section, "name_hex", property.name, error) ||
            !ParseEnum(section, "type", property.type, error) ||
            !ParseBool(section, "boolean", property.booleanValue, error) ||
            !ParseInt64(section, "integer", property.integerValue, error) ||
            !ParseFloat(section, "number", property.numberValue, error) ||
            !ParseFloat(section, "x", property.x, error) ||
            !ParseFloat(section, "y", property.y, error) ||
            !ParseFloat(section, "z", property.z, error) ||
            !ParseFloat(section, "w", property.w, error) ||
            !ReadHexText(section, "text_hex", property.textValue, error) ||
            !ReadHexText(section, "path_hint_hex", property.pathHint, error))
        {
            return false;
        }
        if (!section.Has("reference_id"))
        {
            error = "Missing property reference_id field.";
            return false;
        }
        property.referenceId = section.GetText("reference_id");
        property.unknownFields = CaptureUnknownFields(section, PropertyKeys());
        return true;
    }

    void WriteDependency(wi::config::Section& section, const ScriptDependency& dependency)
    {
        section.Set("kind", renegade::bridge::ScriptDependencyKindName(dependency.kind));
        section.Set("id", dependency.id);
        section.Set("path_hint_hex", HexEncode(dependency.pathHint));
        section.Set("optional", dependency.optional);
        ApplyUnknownFields(section, dependency.unknownFields, DependencyKeys());
    }

    bool ReadDependency(
        const wi::config::Section& section,
        ScriptDependency& dependency,
        std::string& error)
    {
        if (!ParseEnum(section, "kind", dependency.kind, error) ||
            !section.Has("id") ||
            !ReadHexText(section, "path_hint_hex", dependency.pathHint, error) ||
            !ParseBool(section, "optional", dependency.optional, error))
        {
            if (error.empty()) error = "Malformed script dependency.";
            return false;
        }
        dependency.id = section.GetText("id");
        dependency.unknownFields = CaptureUnknownFields(section, DependencyKeys());
        return true;
    }

    bool ReadAttachment(
        wi::config::File& file,
        const std::size_t index,
        ScriptAttachment& attachment,
        std::unordered_set<std::string>& consumedSections,
        std::string& error)
    {
        const std::string sectionName = AttachmentSection(index);
        if (!file.HasSection(sectionName.c_str()))
        {
            error = "Script document is missing " + sectionName + ".";
            return false;
        }
        consumedSections.insert(sectionName);
        auto& section = file.GetSection(sectionName.c_str());

        if (!section.Has("instance_id") || !section.Has("owner_entity_id") ||
            !section.Has("source_id") ||
            !ReadHexText(section, "source_path_hex", attachment.sourcePath, error) ||
            !ParseEnum(section, "scope", attachment.scope, error) ||
            !ParseEnum(section, "presentation", attachment.presentation, error) ||
            !ParseBool(section, "enabled", attachment.enabled, error) ||
            !ParseUnsigned(section, "order", attachment.order, error) ||
            !ParseUnsigned(section, "api_version", attachment.apiVersion, error) ||
            !ParseBool(section, "unsafe", attachment.unsafe, error) ||
            !ReadProvenance(section, attachment.provenance, error))
        {
            if (error.empty()) error = "Malformed script attachment section: " + sectionName;
            return false;
        }

        attachment.scriptInstanceId = section.GetText("instance_id");
        attachment.ownerEntityId = section.GetText("owner_entity_id");
        attachment.sourceId = section.GetText("source_id");
        attachment.unknownFields = CaptureUnknownFields(section, AttachmentKeys());
        attachment.provenance.unknownFields = attachment.unknownFields;
        for (const auto& key : AttachmentKeys())
            attachment.provenance.unknownFields.erase(key);

        std::size_t dependencyCount = 0;
        std::size_t propertyCount = 0;
        std::size_t capabilityCount = 0;
        if (!ParseCount(section, "dependency_count", MaximumNestedCount, dependencyCount, error) ||
            !ParseCount(section, "property_count", MaximumNestedCount, propertyCount, error) ||
            !ParseCount(section, "capability_count", MaximumNestedCount, capabilityCount, error))
        {
            return false;
        }

        attachment.dependencies.clear();
        attachment.dependencies.reserve(dependencyCount);
        for (std::size_t dependencyIndex = 0; dependencyIndex < dependencyCount; ++dependencyIndex)
        {
            const std::string dependencyName = DependencySection(index, dependencyIndex);
            if (!file.HasSection(dependencyName.c_str()))
            {
                error = "Script document is missing " + dependencyName + ".";
                return false;
            }
            consumedSections.insert(dependencyName);
            ScriptDependency dependency;
            if (!ReadDependency(file.GetSection(dependencyName.c_str()), dependency, error))
                return false;
            attachment.dependencies.push_back(std::move(dependency));
        }

        attachment.properties.clear();
        attachment.properties.reserve(propertyCount);
        for (std::size_t propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex)
        {
            const std::string propertyName = PropertySection(index, propertyIndex);
            if (!file.HasSection(propertyName.c_str()))
            {
                error = "Script document is missing " + propertyName + ".";
                return false;
            }
            consumedSections.insert(propertyName);
            ScriptPropertyValue property;
            if (!ReadProperty(file.GetSection(propertyName.c_str()), property, error))
                return false;
            attachment.properties.push_back(std::move(property));
        }

        attachment.capabilities.clear();
        attachment.capabilities.reserve(capabilityCount);
        for (std::size_t capabilityIndex = 0; capabilityIndex < capabilityCount; ++capabilityIndex)
        {
            const std::string capabilityName = CapabilitySection(index, capabilityIndex);
            if (!file.HasSection(capabilityName.c_str()))
            {
                error = "Script document is missing " + capabilityName + ".";
                return false;
            }
            consumedSections.insert(capabilityName);
            auto& capability = file.GetSection(capabilityName.c_str());
            std::string value;
            if (!ReadHexText(capability, "value_hex", value, error))
                return false;
            attachment.capabilities.push_back(std::move(value));
        }
        return true;
    }

    void WriteAttachment(
        wi::config::File& file,
        const std::size_t index,
        const ScriptAttachment& attachment,
        std::unordered_set<std::string>& reservedSections)
    {
        const std::string sectionName = AttachmentSection(index);
        reservedSections.insert(sectionName);
        auto& section = file.GetSection(sectionName.c_str());
        section.Set("instance_id", attachment.scriptInstanceId);
        section.Set("scope", renegade::bridge::ScriptScopeName(attachment.scope));
        section.Set("owner_entity_id", attachment.ownerEntityId);
        section.Set("source_id", attachment.sourceId);
        section.Set("source_path_hex", HexEncode(attachment.sourcePath));
        section.Set("presentation", renegade::bridge::ScriptPresentationName(attachment.presentation));
        section.Set("enabled", attachment.enabled);
        section.Set("order", attachment.order);
        section.Set("api_version", attachment.apiVersion);
        section.Set("unsafe", attachment.unsafe);
        section.Set("dependency_count", static_cast<std::uint32_t>(attachment.dependencies.size()));
        section.Set("property_count", static_cast<std::uint32_t>(attachment.properties.size()));
        section.Set("capability_count", static_cast<std::uint32_t>(attachment.capabilities.size()));
        WriteProvenance(section, attachment.provenance);
        ApplyUnknownFields(section, attachment.unknownFields, AttachmentKeys());

        for (std::size_t dependencyIndex = 0; dependencyIndex < attachment.dependencies.size(); ++dependencyIndex)
        {
            const std::string dependencyName = DependencySection(index, dependencyIndex);
            reservedSections.insert(dependencyName);
            WriteDependency(file.GetSection(dependencyName.c_str()), attachment.dependencies[dependencyIndex]);
        }
        for (std::size_t propertyIndex = 0; propertyIndex < attachment.properties.size(); ++propertyIndex)
        {
            const std::string propertyName = PropertySection(index, propertyIndex);
            reservedSections.insert(propertyName);
            WriteProperty(file.GetSection(propertyName.c_str()), attachment.properties[propertyIndex]);
        }
        for (std::size_t capabilityIndex = 0; capabilityIndex < attachment.capabilities.size(); ++capabilityIndex)
        {
            const std::string capabilityName = CapabilitySection(index, capabilityIndex);
            reservedSections.insert(capabilityName);
            auto& capability = file.GetSection(capabilityName.c_str());
            capability.Set("value_hex", HexEncode(attachment.capabilities[capabilityIndex]));
        }
    }
}

namespace renegade::bridge
{
    const char* ScriptScopeName(const ScriptScope value) noexcept
    {
        switch (value)
        {
        case ScriptScope::Entity: return "entity";
        case ScriptScope::Level: return "level";
        case ScriptScope::Game: return "game";
        default: return "unknown";
        }
    }

    const char* ScriptPresentationName(const ScriptPresentation value) noexcept
    {
        switch (value)
        {
        case ScriptPresentation::Action: return "action";
        case ScriptPresentation::Script: return "script";
        case ScriptPresentation::GlobalScript: return "global_script";
        default: return "unknown";
        }
    }

    const char* ScriptPresentationLabel(const ScriptPresentation value) noexcept
    {
        switch (value)
        {
        case ScriptPresentation::Action: return "ACTION";
        case ScriptPresentation::Script: return "SCRIPT";
        case ScriptPresentation::GlobalScript: return "GLOBAL SCRIPT";
        default: return "UNKNOWN";
        }
    }

    const char* ScriptPropertyTypeName(const ScriptPropertyType value) noexcept
    {
        switch (value)
        {
        case ScriptPropertyType::Boolean: return "boolean";
        case ScriptPropertyType::Integer: return "integer";
        case ScriptPropertyType::Float: return "float";
        case ScriptPropertyType::String: return "string";
        case ScriptPropertyType::Colour: return "colour";
        case ScriptPropertyType::Vector2: return "vector2";
        case ScriptPropertyType::Vector3: return "vector3";
        case ScriptPropertyType::EntityReference: return "entity_ref";
        case ScriptPropertyType::AssetReference: return "asset_ref";
        case ScriptPropertyType::Animation: return "animation";
        case ScriptPropertyType::Audio: return "audio";
        case ScriptPropertyType::Enum: return "enum";
        default: return "unknown";
        }
    }

    const char* ScriptDependencyKindName(const ScriptDependencyKind value) noexcept
    {
        switch (value)
        {
        case ScriptDependencyKind::ScriptModule: return "script_module";
        case ScriptDependencyKind::Asset: return "asset";
        default: return "unknown";
        }
    }

    const char* ScriptProvenanceKindName(const ScriptProvenanceKind value) noexcept
    {
        switch (value)
        {
        case ScriptProvenanceKind::Project: return "project";
        case ScriptProvenanceKind::InstalledLibrary: return "installed_library";
        case ScriptProvenanceKind::PersonalLibrary: return "personal_library";
        default: return "unknown";
        }
    }

    ScriptDocument CreateScriptDocument(
        const StableId& projectId,
        const StableId& sceneDocumentId,
        std::string scenePathHint,
        std::string generatorVersion)
    {
        ScriptDocument document;
        document.sceneDocumentId = sceneDocumentId;
        document.scenePathHint = fs::u8path(scenePathHint).lexically_normal().generic_u8string();
        document.envelope = CreateDocumentEnvelope(
            projectId,
            ScriptDocumentType,
            ScriptDocumentPathHintForScene(document.scenePathHint),
            std::move(generatorVersion));
        return document;
    }

    std::string ScriptDocumentPathForScene(const std::string& scenePath)
    {
        return fs::u8path(scenePath).lexically_normal().generic_u8string() + ScriptDocumentSuffix;
    }

    std::string ScriptDocumentPathHintForScene(const std::string& scenePathHint)
    {
        return ScriptDocumentPathForScene(scenePathHint);
    }

    ScriptSourceBinding CaptureScriptSourceBinding(const ScriptAttachment& attachment)
    {
        ScriptSourceBinding source;
        source.sourceId = attachment.sourceId;
        source.sourcePath = attachment.sourcePath;
        source.presentation = attachment.presentation;
        source.apiVersion = attachment.apiVersion;
        source.unsafe = attachment.unsafe;
        source.provenance = attachment.provenance;
        source.dependencies = attachment.dependencies;
        source.capabilities = attachment.capabilities;
        return source;
    }

    ScriptAttachment CreateScriptAttachment(
        const ScriptScope scope,
        StableId ownerEntityId,
        const ScriptSourceBinding& source)
    {
        ScriptAttachment attachment;
        attachment.scriptInstanceId = GenerateStableId();
        attachment.scope = scope;
        attachment.ownerEntityId = std::move(ownerEntityId);
        attachment.sourceId = IsValidStableId(source.sourceId)
            ? source.sourceId
            : GenerateStableId();
        attachment.sourcePath = fs::u8path(source.sourcePath).lexically_normal().generic_u8string();
        attachment.presentation = source.presentation;
        attachment.apiVersion = source.apiVersion;
        attachment.unsafe = source.unsafe;
        attachment.provenance = source.provenance;
        attachment.dependencies = source.dependencies;
        attachment.capabilities = source.capabilities;
        return attachment;
    }

    ScriptAttachment* FindScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId) noexcept
    {
        const auto found = std::find_if(
            document.attachments.begin(), document.attachments.end(),
            [&](const ScriptAttachment& attachment)
            {
                return attachment.scriptInstanceId == scriptInstanceId;
            });
        return found == document.attachments.end() ? nullptr : &*found;
    }

    const ScriptAttachment* FindScriptAttachment(
        const ScriptDocument& document,
        const StableId& scriptInstanceId) noexcept
    {
        const auto found = std::find_if(
            document.attachments.begin(), document.attachments.end(),
            [&](const ScriptAttachment& attachment)
            {
                return attachment.scriptInstanceId == scriptInstanceId;
            });
        return found == document.attachments.end() ? nullptr : &*found;
    }

    bool ValidateScriptDocument(const ScriptDocument& document, std::string& error)
    {
        if (!ValidateDocumentEnvelope(document.envelope, error))
            return false;
        if (document.envelope.documentType != ScriptDocumentType)
        {
            error = "Script companion envelope must use document type scene-scripts.";
            return false;
        }
        if (document.schemaVersion != ScriptDocument::CurrentSchemaVersion)
        {
            error = "Unsupported script document schema version: " +
                std::to_string(document.schemaVersion);
            return false;
        }
        if (!IsValidStableId(document.sceneDocumentId))
        {
            error = "Script document is missing the owning Scene document ID.";
            return false;
        }
        if (!IsContentRelativePath(document.scenePathHint) ||
            LowerAscii(fs::u8path(document.scenePathHint).extension().generic_u8string()) != ".wiscene")
        {
            error = "Script document Scene path hint must identify a project Content/*.wiscene.";
            return false;
        }
        if (document.envelope.pathHint != ScriptDocumentPathHintForScene(document.scenePathHint))
        {
            error = "Script companion path hint does not match its owning Scene path hint.";
            return false;
        }
        if (document.attachments.size() > MaximumAttachmentCount)
        {
            error = "Script document attachment count exceeds the S2 safety limit.";
            return false;
        }

        std::unordered_set<StableId> instanceIds;
        std::unordered_map<StableId, const ScriptAttachment*> sourcesById;
        std::unordered_map<std::string, StableId> sourceIdsByPath;
        std::unordered_map<std::string, std::vector<std::uint32_t>> groupOrders;

        for (const auto& attachment : document.attachments)
        {
            if (!ValidateAttachment(attachment, error))
                return false;
            if (!instanceIds.insert(attachment.scriptInstanceId).second)
            {
                error = "Duplicate ScriptInstanceId in script document: " +
                    attachment.scriptInstanceId;
                return false;
            }

            const auto source = sourcesById.find(attachment.sourceId);
            if (source == sourcesById.end())
            {
                sourcesById.emplace(attachment.sourceId, &attachment);
            }
            else if (!SameSourceAuthority(*source->second, attachment))
            {
                error = "Shared sourceId has conflicting source authority: " +
                    attachment.sourceId;
                return false;
            }

            const std::string normalizedPath =
                fs::u8path(attachment.sourcePath).lexically_normal().generic_u8string();
            const auto pathSource = sourceIdsByPath.find(normalizedPath);
            if (pathSource == sourceIdsByPath.end())
            {
                sourceIdsByPath.emplace(normalizedPath, attachment.sourceId);
            }
            else if (pathSource->second != attachment.sourceId)
            {
                error = "One governed script source path maps to multiple sourceIds: " +
                    normalizedPath;
                return false;
            }
            groupOrders[OrderGroup(attachment)].push_back(attachment.order);
        }

        for (auto& group : groupOrders)
        {
            auto& orders = group.second;
            std::sort(orders.begin(), orders.end());
            for (std::size_t index = 0; index < orders.size(); ++index)
            {
                if (orders[index] != index)
                {
                    error = "Script attachment order must be dense and unique within each owner scope.";
                    return false;
                }
            }
        }

        error.clear();
        return true;
    }

    bool ValidateScriptDocumentAgainstScene(
        const ScriptDocument& document,
        const wi::scene::Scene& scene,
        std::string& error)
    {
        if (!ValidateScriptDocument(document, error))
            return false;

        EntityIdentityIndex identities;
        if (!identities.Build(scene, error))
            return false;
        for (const auto& attachment : document.attachments)
        {
            if (attachment.scope == ScriptScope::Entity &&
                identities.Resolve(attachment.ownerEntityId) == wi::ecs::INVALID_ENTITY)
            {
                error = "Entity script attachment owner does not resolve in the Scene: " +
                    attachment.ownerEntityId;
                return false;
            }
        }

        // Entity-reference properties intentionally are not resolved here.
        // Keeping a valid stable ID even while its target is missing preserves
        // unresolved Inspector references for later repair instead of deleting state.
        error.clear();
        return true;
    }

    bool WriteScriptDocument(
        const std::string& filePath,
        const ScriptDocument& document,
        std::string& error)
    {
        ScriptDocument candidate = document;
        NormalizeScriptAttachmentOrder(candidate);
        if (!ValidateScriptDocument(candidate, error))
            return false;

        const auto writer = [candidate](wi::config::File& file)
        {
            const std::unordered_set<std::string> rootReserved = { "format", "version" };
            auto& root = static_cast<wi::config::Section&>(file);
            ApplyUnknownFields(root, candidate.unknownRootFields, rootReserved);

            auto& envelope = file.GetSection("document");
            ApplyUnknownFields(envelope, candidate.unknownDocumentFields, EnvelopeKeys());

            auto& script = file.GetSection("script_document");
            script.Set("format", ScriptDocumentFormat);
            script.Set("schema_version", candidate.schemaVersion);
            script.Set("scene_document_id", candidate.sceneDocumentId);
            script.Set("scene_path_hex", HexEncode(candidate.scenePathHint));
            script.Set("attachment_count", static_cast<std::uint32_t>(candidate.attachments.size()));
            ApplyUnknownFields(
                script,
                candidate.unknownScriptDocumentFields,
                ScriptDocumentKeys());

            std::unordered_set<std::string> reservedSections = {
                "document", "script_document"
            };
            for (std::size_t index = 0; index < candidate.attachments.size(); ++index)
                WriteAttachment(file, index, candidate.attachments[index], reservedSections);

            std::vector<std::string> sectionNames;
            sectionNames.reserve(candidate.unknownSections.size());
            for (const auto& item : candidate.unknownSections)
            {
                if (reservedSections.count(item.first) == 0)
                    sectionNames.push_back(item.first);
            }
            std::sort(sectionNames.begin(), sectionNames.end());
            for (const auto& sectionName : sectionNames)
            {
                auto& unknown = file.GetSection(sectionName.c_str());
                static const std::unordered_set<std::string> none;
                ApplyUnknownFields(
                    unknown,
                    candidate.unknownSections.at(sectionName),
                    none);
            }
        };

        const auto validator = [candidate](
            const std::string& stagedPath,
            std::string& validationError)
        {
            ScriptDocument roundTrip;
            if (!ReadScriptDocument(
                    stagedPath,
                    candidate.envelope.projectId,
                    candidate.sceneDocumentId,
                    roundTrip,
                    validationError))
            {
                return false;
            }
            if (roundTrip.envelope.documentId != candidate.envelope.documentId)
            {
                validationError = "Script companion document ID changed during transactional round-trip.";
                return false;
            }
            return true;
        };

        return WriteTransactionalDocument(
            filePath,
            candidate.envelope,
            false,
            writer,
            validator,
            error);
    }

    bool ReadScriptDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        const StableId& expectedSceneDocumentId,
        ScriptDocument& document,
        std::string& error)
    {
        if (!IsValidStableId(expectedProjectId) ||
            !IsValidStableId(expectedSceneDocumentId))
        {
            error = "Valid project and Scene document IDs are required to read a script companion.";
            return false;
        }

        DocumentEnvelope envelope;
        if (!ReadDocumentEnvelope(filePath, envelope, error))
            return false;
        if (envelope.projectId != expectedProjectId ||
            envelope.documentType != ScriptDocumentType)
        {
            error = "Script companion envelope does not belong to the expected project/type.";
            return false;
        }

        wi::config::File file;
        if (!file.Open(filePath))
        {
            error = "Could not open script companion: " + filePath;
            return false;
        }
        if (!file.HasSection("script_document"))
        {
            error = "Script companion is missing [script_document].";
            return false;
        }

        ScriptDocument parsed;
        parsed.envelope = envelope;
        auto& script = file.GetSection("script_document");
        if (!script.Has("format") || script.GetText("format") != ScriptDocumentFormat ||
            !ParseUnsigned(script, "schema_version", parsed.schemaVersion, error) ||
            !script.Has("scene_document_id") ||
            !ReadHexText(script, "scene_path_hex", parsed.scenePathHint, error))
        {
            if (error.empty()) error = "Malformed script_document authority section.";
            return false;
        }
        parsed.sceneDocumentId = script.GetText("scene_document_id");
        if (parsed.sceneDocumentId != expectedSceneDocumentId)
        {
            error = "Script companion targets a different Scene document ID.";
            return false;
        }

        std::size_t attachmentCount = 0;
        if (!ParseCount(script, "attachment_count", MaximumAttachmentCount, attachmentCount, error))
            return false;
        parsed.unknownScriptDocumentFields =
            CaptureUnknownFields(script, ScriptDocumentKeys());

        const auto& root = static_cast<const wi::config::Section&>(file);
        parsed.unknownRootFields = CaptureUnknownFields(
            root,
            std::unordered_set<std::string>{ "format", "version" });
        if (file.HasSection("document"))
        {
            parsed.unknownDocumentFields = CaptureUnknownFields(
                file.GetSection("document"), EnvelopeKeys());
        }

        std::unordered_set<std::string> consumedSections = {
            "document", "script_document"
        };
        parsed.attachments.reserve(attachmentCount);
        for (std::size_t index = 0; index < attachmentCount; ++index)
        {
            ScriptAttachment attachment;
            if (!ReadAttachment(file, index, attachment, consumedSections, error))
                return false;
            parsed.attachments.push_back(std::move(attachment));
        }

        for (const auto& item : file.GetSortedSections())
        {
            if (consumedSections.count(item.first) != 0)
                continue;
            std::unordered_map<std::string, std::string> fields;
            for (const auto& field : *item.second)
                fields.emplace(field.first, field.second);
            parsed.unknownSections.emplace(item.first, std::move(fields));
        }

        if (!ValidateScriptDocument(parsed, error))
            return false;

        document = std::move(parsed);
        error.clear();
        return true;
    }

    bool ValidateProjectScriptSource(
        const std::string& projectRoot,
        const std::string& projectRelativePath,
        std::string& error,
        const std::size_t maximumBytes)
    {
        if (projectRoot.empty() || !IsScriptSourcePath(projectRelativePath))
        {
            error = "Script source must be a project-relative Content/Scripts/*.lua path.";
            return false;
        }
        if (maximumBytes == 0)
        {
            error = "Script source size limit must be greater than zero.";
            return false;
        }

        std::error_code pathError;
        const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), pathError);
        if (pathError || root.empty())
        {
            error = "Could not canonicalize script project root: " + pathError.message();
            return false;
        }

        const fs::path relative = fs::u8path(projectRelativePath).lexically_normal();
        fs::path current = root;
        for (const auto& part : relative)
        {
            current /= part;
            const fs::file_status status = fs::symlink_status(current, pathError);
            if (pathError)
            {
                error = "Could not inspect script source path: " + pathError.message();
                return false;
            }
            if (fs::is_symlink(status))
            {
                error = "Script sources may not traverse symbolic links.";
                return false;
            }
        }

        const fs::path source = fs::weakly_canonical(root / relative, pathError);
        if (pathError || !fs::is_regular_file(source, pathError))
        {
            error = "Script source does not resolve to a regular file.";
            return false;
        }
        if (pathError)
        {
            error = "Could not inspect script source file: " + pathError.message();
            return false;
        }

        const fs::path inside = fs::relative(source, root, pathError);
        if (pathError || inside.empty() || inside.is_absolute() ||
            std::any_of(inside.begin(), inside.end(), [](const fs::path& part) { return part == ".."; }))
        {
            error = "Script source resolves outside the project root.";
            return false;
        }

        const std::uintmax_t size = fs::file_size(source, pathError);
        if (pathError || size > maximumBytes)
        {
            error = pathError
                ? "Could not inspect script source size: " + pathError.message()
                : "Script source exceeds the governed size limit of " +
                    std::to_string(maximumBytes) + " bytes.";
            return false;
        }

        error.clear();
        return true;
    }
}
