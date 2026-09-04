#pragma once

#include "renegade/bridge/ScriptDocumentService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr std::uint32_t ScriptMetadataSchemaVersion = 1;
    inline constexpr std::size_t ScriptMetadataDefaultMemoryBudgetBytes =
        8u * 1024u * 1024u;
    inline constexpr std::size_t ScriptMetadataDefaultInstructionBudget =
        100000u;

    enum class ScriptMetadataDiagnosticSeverity : std::uint8_t
    {
        Warning,
        Error,
    };

    struct ScriptMetadataDiagnostic
    {
        ScriptMetadataDiagnosticSeverity severity =
            ScriptMetadataDiagnosticSeverity::Error;
        std::string code;
        std::string sourcePath;
        std::string field;
        std::string message;
        int line = 0;
    };

    struct ScriptMetadataEnumOption
    {
        std::string value;
        std::string label;
    };

    struct ScriptMetadataPropertyDescriptor
    {
        std::string name;
        std::string label;
        std::string description;
        ScriptPropertyType type = ScriptPropertyType::String;

        ScriptPropertyValue defaultValue;
        bool hasDefault = false;

        bool hasMinimum = false;
        double minimum = 0.0;
        bool hasMaximum = false;
        double maximum = 0.0;
        bool hasStep = false;
        double step = 0.0;

        std::vector<ScriptMetadataEnumOption> enumOptions;
    };

    struct ScriptMetadataDescriptor
    {
        std::uint32_t schemaVersion = ScriptMetadataSchemaVersion;
        std::string name;
        std::string description;
        std::string category;
        ScriptPresentation presentation = ScriptPresentation::Script;
        std::vector<ScriptMetadataPropertyDescriptor> properties;
    };

    struct ScriptMetadataEvaluationResult
    {
        bool succeeded = false;
        ScriptMetadataDescriptor descriptor;
        std::vector<ScriptMetadataDiagnostic> diagnostics;
    };

    // S4A authoring contract. The guard is part of the frozen schema-v1 idiom:
    // S4A's metadata-only state supplies renegade.metadata; the S3 gameplay
    // Runtime does not, so the same source simply skips this authoring block.
    //
    // if renegade and renegade.metadata then
    //   renegade.metadata({
    //     schema_version = 1,
    //     name = "Open Door",
    //     description = "Opens an entity when activated.",
    //     category = "Interaction",
    //     role = "ACTION", -- ACTION | SCRIPT | GLOBAL SCRIPT
    //     properties = {
    //       { name="speed", label="Speed", type="float", default=2.0 },
    //     },
    //   })
    // end
    //
    // The declaration is captured in a dedicated metadata-only Lua state and
    // evaluation terminates immediately after capture. Gameplay setup below
    // the declaration is never executed by this service.
    [[nodiscard]] ScriptMetadataEvaluationResult EvaluateScriptMetadata(
        const std::string& projectRoot,
        const std::string& projectRelativeSourcePath,
        std::size_t memoryBudgetBytes =
            ScriptMetadataDefaultMemoryBudgetBytes,
        std::size_t instructionBudget =
            ScriptMetadataDefaultInstructionBudget);

    // Seeds one S2 ScriptAttachment with validated metadata defaults. Existing
    // persisted property values win. Reference-backed properties intentionally
    // have no source-authored raw-ID defaults and remain unresolved until
    // Studio supplies an opaque reference.
    [[nodiscard]] bool ApplyScriptMetadataDefaults(
        const ScriptMetadataDescriptor& metadata,
        ScriptAttachment& attachment,
        std::string& error);
}
