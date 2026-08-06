#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <wiConfig.h>
#include <wiHelper.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* DocumentFormat = "renegade-document";

    bool IsLowerHex(const char value) noexcept
    {
        return (value >= '0' && value <= '9') ||
            (value >= 'a' && value <= 'f');
    }

    bool IsSafePathHint(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        const fs::path path = fs::u8path(value);
        if (path.is_absolute() || path.has_root_name() ||
            path.has_root_directory())
        {
            return false;
        }

        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    std::string EntityLabel(const wi::ecs::Entity entity)
    {
        return std::to_string(static_cast<std::uint64_t>(entity));
    }
}

namespace renegade::bridge
{
    bool IsValidStableId(const StableId& value) noexcept
    {
        if (value.size() != 36 || value[8] != '-' || value[13] != '-' ||
            value[18] != '-' || value[23] != '-')
        {
            return false;
        }

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index == 8 || index == 13 || index == 18 || index == 23)
            {
                continue;
            }
            if (!IsLowerHex(value[index]))
            {
                return false;
            }
        }

        // Generated IDs use UUID version 4 and the RFC 4122 variant. Keeping
        // these bits constrained prevents arbitrary 36-character strings from
        // becoming identity authorities.
        return value[14] == '4' &&
            (value[19] == '8' || value[19] == '9' ||
             value[19] == 'a' || value[19] == 'b');
    }

    StableId GenerateStableId()
    {
        thread_local std::mt19937_64 generator([]
        {
            std::random_device source;
            std::seed_seq seed{
                source(), source(), source(), source(),
                source(), source(), source(), source()
            };
            return std::mt19937_64(seed);
        }());
        std::uniform_int_distribution<unsigned int> byte(0, 255);

        std::array<unsigned char, 16> value{};
        for (auto& item : value)
        {
            item = static_cast<unsigned char>(byte(generator));
        }
        value[6] = static_cast<unsigned char>((value[6] & 0x0Fu) | 0x40u);
        value[8] = static_cast<unsigned char>((value[8] & 0x3Fu) | 0x80u);

        std::ostringstream stream;
        stream << std::hex << std::nouppercase << std::setfill('0');
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index == 4 || index == 6 || index == 8 || index == 10)
            {
                stream << '-';
            }
            stream << std::setw(2) << static_cast<unsigned int>(value[index]);
        }
        return stream.str();
    }

    DocumentEnvelope CreateDocumentEnvelope(
        const StableId& projectId,
        std::string documentType,
        std::string pathHint,
        std::string generatorVersion)
    {
        DocumentEnvelope envelope;
        envelope.documentId = GenerateStableId();
        envelope.projectId = projectId;
        envelope.documentType = std::move(documentType);
        envelope.pathHint = std::move(pathHint);
        envelope.generatorVersion = std::move(generatorVersion);
        return envelope;
    }

    bool ValidateDocumentEnvelope(
        const DocumentEnvelope& envelope,
        std::string& error)
    {
        if (envelope.formatIdentifier != DocumentFormat)
        {
            error = "The document envelope has an unsupported format identifier.";
            return false;
        }
        if (envelope.schemaVersion != DocumentEnvelope::CurrentSchemaVersion)
        {
            error = "Unsupported Renegade document schema version: " +
                std::to_string(envelope.schemaVersion);
            return false;
        }
        if (!IsValidStableId(envelope.documentId))
        {
            error = "The document envelope is missing a valid document ID.";
            return false;
        }
        if (!IsValidStableId(envelope.projectId))
        {
            error = "The document envelope is missing a valid owning project ID.";
            return false;
        }
        if (envelope.documentType.empty())
        {
            error = "The document envelope is missing its document type.";
            return false;
        }
        if (!IsSafePathHint(envelope.pathHint))
        {
            error = "The document envelope path hint must be project-relative.";
            return false;
        }
        if (envelope.generatorVersion.empty())
        {
            error = "The document envelope is missing its generator version.";
            return false;
        }

        error.clear();
        return true;
    }

    bool WriteDocumentEnvelope(
        const std::string& filePath,
        const DocumentEnvelope& envelope,
        std::string& error)
    {
        if (filePath.empty())
        {
            error = "A document-envelope path is required.";
            return false;
        }
        if (!ValidateDocumentEnvelope(envelope, error))
        {
            return false;
        }

        try
        {
            const fs::path path = fs::u8path(filePath).lexically_normal();
            if (!path.parent_path().empty())
            {
                fs::create_directories(path.parent_path());
            }

            wi::config::File file;
            file.Open(path.generic_u8string());
            file.Set("format", envelope.formatIdentifier);
            file.Set("version", envelope.schemaVersion);
            auto& document = file.GetSection("document");
            document.Set("id", envelope.documentId);
            document.Set("project_id", envelope.projectId);
            document.Set("type", envelope.documentType);
            document.Set("path_hint", envelope.pathHint);
            document.Set("generator", envelope.generatorVersion);
            document.Set(
                "migrated_from",
                static_cast<int>(envelope.migratedFromVersion));
            file.Commit();

            if (!wi::helper::FileExists(path.generic_u8string()))
            {
                error = "Could not write document envelope: " +
                    path.generic_u8string();
                return false;
            }

            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not write document envelope: ") +
                exception.what();
            return false;
        }
    }

    bool ReadDocumentEnvelope(
        const std::string& filePath,
        DocumentEnvelope& envelope,
        std::string& error)
    {
        if (filePath.empty())
        {
            error = "A document-envelope path is required.";
            return false;
        }

        try
        {
            const fs::path path = fs::u8path(filePath).lexically_normal();
            wi::config::File file;
            if (!file.Open(path.generic_u8string()))
            {
                error = "Could not read document envelope: " +
                    path.generic_u8string();
                return false;
            }
            if (!file.HasSection("document"))
            {
                error = "The document envelope is missing its document section.";
                return false;
            }

            DocumentEnvelope parsed;
            parsed.formatIdentifier = file.GetText("format");
            parsed.schemaVersion =
                static_cast<std::uint32_t>(file.GetInt("version"));
            const auto& document = file.GetSection("document");
            parsed.documentId = document.GetText("id");
            parsed.projectId = document.GetText("project_id");
            parsed.documentType = document.GetText("type");
            parsed.pathHint = document.GetText("path_hint");
            parsed.generatorVersion = document.GetText("generator");
            parsed.migratedFromVersion = static_cast<std::uint32_t>(
                std::max(0, document.GetInt("migrated_from")));

            if (!ValidateDocumentEnvelope(parsed, error))
            {
                return false;
            }

            envelope = std::move(parsed);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not read document envelope: ") +
                exception.what();
            return false;
        }
    }

    bool RetargetDocumentEnvelope(
        DocumentEnvelope& envelope,
        const std::string& pathHint,
        std::string& error)
    {
        if (!IsSafePathHint(pathHint))
        {
            error = "The document path hint must be project-relative.";
            return false;
        }

        envelope.pathHint = fs::u8path(pathHint).lexically_normal()
            .generic_u8string();
        return ValidateDocumentEnvelope(envelope, error);
    }

    bool ValidateDocumentEnvelopes(
        const StableId& expectedProjectId,
        const std::vector<DocumentEnvelope>& envelopes,
        std::string& error)
    {
        if (!IsValidStableId(expectedProjectId))
        {
            error = "A valid project ID is required to validate documents.";
            return false;
        }

        std::unordered_map<StableId, std::string> seen;
        for (const auto& envelope : envelopes)
        {
            if (!ValidateDocumentEnvelope(envelope, error))
            {
                return false;
            }
            if (envelope.projectId != expectedProjectId)
            {
                error = "Document '" + envelope.pathHint +
                    "' belongs to a different project ID.";
                return false;
            }

            const auto inserted = seen.emplace(
                envelope.documentId,
                envelope.pathHint);
            if (!inserted.second)
            {
                error = "Duplicate document ID '" + envelope.documentId +
                    "' is used by both '" + inserted.first->second +
                    "' and '" + envelope.pathHint + "'.";
                return false;
            }
        }

        error.clear();
        return true;
    }

    std::string EntityIdentityValidation::Summary() const
    {
        if (issues.empty())
        {
            return {};
        }
        return issues.front().message;
    }

    std::vector<wi::ecs::Entity> EnumeratePersistentSceneEntities(
        const wi::scene::Scene& scene)
    {
        wi::unordered_set<wi::ecs::Entity> discovered;
        scene.FindAllEntities(discovered);

        std::vector<wi::ecs::Entity> entities;
        entities.reserve(discovered.size());
        for (const auto entity : discovered)
        {
            if (entity != wi::ecs::INVALID_ENTITY)
            {
                entities.push_back(entity);
            }
        }
        std::sort(entities.begin(), entities.end());
        return entities;
    }

    StableId PersistentEntityId(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr ||
            !metadata->string_values.has(PersistentEntityIdMetadataKey))
        {
            return {};
        }
        return metadata->string_values.get(PersistentEntityIdMetadataKey);
    }

    bool AssignPersistentEntityId(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const StableId& id,
        std::string& error)
    {
        const auto entities = EnumeratePersistentSceneEntities(scene);
        if (entity == wi::ecs::INVALID_ENTITY ||
            !std::binary_search(entities.begin(), entities.end(), entity))
        {
            error = "Cannot assign a persistent ID to a missing scene entity.";
            return false;
        }
        if (!IsValidStableId(id))
        {
            error = "Cannot assign a malformed persistent entity ID.";
            return false;
        }

        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto other = scene.metadatas.GetEntity(index);
            if (other == entity)
            {
                continue;
            }
            if (scene.metadatas[index].string_values.get(
                    PersistentEntityIdMetadataKey) == id)
            {
                error = "Persistent entity ID '" + id +
                    "' is already assigned to entity " + EntityLabel(other) + ".";
                return false;
            }
        }

        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
        {
            metadata = &scene.metadatas.Create(entity);
        }
        metadata->string_values.set(PersistentEntityIdMetadataKey, id);
        error.clear();
        return true;
    }

    bool AssignNewPersistentEntityId(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        std::string& error)
    {
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            const StableId candidate = GenerateStableId();
            bool used = false;
            for (std::size_t index = 0;
                index < scene.metadatas.GetCount(); ++index)
            {
                if (scene.metadatas[index].string_values.get(
                        PersistentEntityIdMetadataKey) == candidate)
                {
                    used = true;
                    break;
                }
            }
            if (!used)
            {
                return AssignPersistentEntityId(scene, entity, candidate, error);
            }
        }

        error = "Could not generate a unique persistent entity ID.";
        return false;
    }

    EntityIdentityValidation ValidatePersistentEntityIdentities(
        const wi::scene::Scene& scene)
    {
        EntityIdentityValidation validation;
        std::unordered_map<StableId, wi::ecs::Entity> seen;

        for (const auto entity : EnumeratePersistentSceneEntities(scene))
        {
            const StableId id = PersistentEntityId(scene, entity);
            if (id.empty())
            {
                validation.issues.push_back({
                    EntityIdentityIssueCode::Missing,
                    entity,
                    wi::ecs::INVALID_ENTITY,
                    {},
                    "Entity " + EntityLabel(entity) +
                        " is missing a persistent Renegade ID."
                });
                continue;
            }
            if (!IsValidStableId(id))
            {
                validation.issues.push_back({
                    EntityIdentityIssueCode::Malformed,
                    entity,
                    wi::ecs::INVALID_ENTITY,
                    id,
                    "Entity " + EntityLabel(entity) +
                        " has malformed persistent ID '" + id + "'."
                });
                continue;
            }

            const auto inserted = seen.emplace(id, entity);
            if (!inserted.second)
            {
                validation.issues.push_back({
                    EntityIdentityIssueCode::Duplicate,
                    entity,
                    inserted.first->second,
                    id,
                    "Entities " + EntityLabel(inserted.first->second) +
                        " and " + EntityLabel(entity) +
                        " share persistent ID '" + id + "'."
                });
            }
        }

        return validation;
    }

    bool EnsurePersistentEntityIdentities(
        wi::scene::Scene& scene,
        std::string& error)
    {
        const auto validation = ValidatePersistentEntityIdentities(scene);
        for (const auto& issue : validation.issues)
        {
            if (issue.code != EntityIdentityIssueCode::Missing)
            {
                error = issue.message;
                return false;
            }
        }

        for (const auto& issue : validation.issues)
        {
            if (!AssignNewPersistentEntityId(scene, issue.entity, error))
            {
                return false;
            }
        }

        const auto finalValidation = ValidatePersistentEntityIdentities(scene);
        if (!finalValidation.IsValid())
        {
            error = finalValidation.Summary();
            return false;
        }

        error.clear();
        return true;
    }

    bool EntityIdentityIndex::Build(
        const wi::scene::Scene& scene,
        std::string& error)
    {
        entitiesById_.clear();
        const auto validation = ValidatePersistentEntityIdentities(scene);
        if (!validation.IsValid())
        {
            error = validation.Summary();
            return false;
        }

        for (const auto entity : EnumeratePersistentSceneEntities(scene))
        {
            entitiesById_.emplace(PersistentEntityId(scene, entity), entity);
        }
        error.clear();
        return true;
    }

    wi::ecs::Entity EntityIdentityIndex::Resolve(
        const StableId& id) const noexcept
    {
        const auto found = entitiesById_.find(id);
        return found == entitiesById_.end()
            ? wi::ecs::INVALID_ENTITY
            : found->second;
    }
}
