#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
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

    void RemoveWithoutThrow(const fs::path& path)
    {
        std::error_code ignored;
        fs::remove(path, ignored);
    }

    bool ReadFileBytes(
        const fs::path& path,
        std::vector<std::uint8_t>& content,
        std::string& error)
    {
        content.clear();

        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            error = "Could not read document content: " +
                path.generic_u8string();
            return false;
        }

        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            error = "Could not inspect document content size: " +
                path.generic_u8string();
            return false;
        }

        content.resize(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!content.empty())
        {
            stream.read(
                reinterpret_cast<char*>(content.data()),
                static_cast<std::streamsize>(content.size()));
            if (stream.gcount() !=
                static_cast<std::streamsize>(content.size()))
            {
                content.clear();
                error = "Could not read complete document content: " +
                    path.generic_u8string();
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool FileMatchesBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& expected,
        std::string& error)
    {
        std::vector<std::uint8_t> actual;
        if (!ReadFileBytes(path, actual, error))
        {
            return false;
        }
        if (actual != expected)
        {
            error = "Document bytes did not match the requested payload: " +
                path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    fs::path PermanentBackupPath(const fs::path& destination)
    {
        // Keep the backup's final extension as .bak so stable-ID document
        // scans never mistake it for a second authoritative Flow/Screen file.
        return destination.parent_path() / fs::u8path(
            destination.filename().generic_u8string() + ".bak");
    }

    bool SameEnvelope(
        const renegade::bridge::DocumentEnvelope& left,
        const renegade::bridge::DocumentEnvelope& right)
    {
        return left.formatIdentifier == right.formatIdentifier &&
            left.schemaVersion == right.schemaVersion &&
            left.documentId == right.documentId &&
            left.projectId == right.projectId &&
            left.documentType == right.documentType &&
            left.pathHint == right.pathHint &&
            left.generatorVersion == right.generatorVersion &&
            left.migratedFromVersion == right.migratedFromVersion;
    }

    void ApplyEnvelope(
        wi::config::File& file,
        const renegade::bridge::DocumentEnvelope& envelope)
    {
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
    }

    bool ResolveTransactionLocation(
        const fs::path& destination,
        const std::string& pathHint,
        fs::path& allowedRoot,
        fs::path& journalDirectory)
    {
        const fs::path relative = fs::u8path(pathHint).lexically_normal();
        fs::path candidate = destination;
        for (const auto& ignored : relative)
        {
            (void)ignored;
            candidate = candidate.parent_path();
        }

        if (!candidate.empty() &&
            (candidate / relative).lexically_normal() == destination)
        {
            allowedRoot = candidate;
            journalDirectory =
                allowedRoot / "Intermediate" / "Transactions";
            return true;
        }

        allowedRoot = destination.parent_path();
        journalDirectory = allowedRoot / ".renegade-transactions";
        return false;
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

    bool WriteTransactionalDocument(
        const std::string& filePath,
        const DocumentEnvelope& envelope,
        const bool preserveExistingSections,
        DocumentContentWriter contentWriter,
        ProjectDocumentValidator validator,
        std::string& error)
    {
        if (filePath.empty())
        {
            error = "A Renegade document path is required.";
            return false;
        }
        if (!ValidateDocumentEnvelope(envelope, error))
        {
            return false;
        }

        fs::path renderPath;
        try
        {
            std::error_code pathError;
            const fs::path destination =
                fs::absolute(fs::u8path(filePath), pathError)
                    .lexically_normal();
            if (pathError || destination.filename().empty())
            {
                error = "Could not resolve the Renegade document path: " +
                    pathError.message();
                return false;
            }

            const fs::path parent = destination.parent_path();
            fs::create_directories(parent, pathError);
            if (pathError)
            {
                error = "Could not create the Renegade document folder: " +
                    pathError.message();
                return false;
            }

            renderPath = parent / fs::u8path(
                destination.filename().generic_u8string() +
                ".renegade-render-" + GenerateStableId());

            const bool destinationExists = fs::exists(destination, pathError);
            if (pathError)
            {
                error = "Could not inspect the Renegade document: " +
                    pathError.message();
                return false;
            }
            if (destinationExists &&
                !fs::is_regular_file(destination, pathError))
            {
                error = "The Renegade document destination is not a file: " +
                    destination.generic_u8string();
                return false;
            }
            if (pathError)
            {
                error = "Could not inspect the Renegade document: " +
                    pathError.message();
                return false;
            }

            if (preserveExistingSections && destinationExists)
            {
                fs::copy_file(
                    destination,
                    renderPath,
                    fs::copy_options::overwrite_existing,
                    pathError);
                if (pathError)
                {
                    error = "Could not prepare the document render copy: " +
                        pathError.message();
                    return false;
                }
            }

            wi::config::File file;
            file.Open(renderPath.generic_u8string());
            ApplyEnvelope(file, envelope);
            if (contentWriter)
            {
                contentWriter(file);
            }
            file.Commit();

            if (!fs::is_regular_file(renderPath))
            {
                error = "Could not render the Renegade document: " +
                    renderPath.generic_u8string();
                return false;
            }

            std::vector<std::uint8_t> content;
            if (!ReadFileBytes(renderPath, content, error))
            {
                return false;
            }
            RemoveWithoutThrow(renderPath);
            renderPath.clear();

            std::vector<std::uint8_t> previous;
            if (destinationExists &&
                !ReadFileBytes(destination, previous, error))
            {
                return false;
            }

            std::vector<ProjectDocumentWrite> writes;
            writes.reserve(
                destinationExists && previous != content ? 2u : 1u);

            if (destinationExists && previous != content)
            {
                ProjectDocumentWrite backup;
                backup.destinationPath =
                    PermanentBackupPath(destination).generic_u8string();
                backup.content = previous;
                backup.validator = [previous](
                    const std::string& path,
                    std::string& validationError)
                {
                    return FileMatchesBytes(
                        fs::u8path(path),
                        previous,
                        validationError);
                };
                writes.push_back(std::move(backup));
            }

            ProjectDocumentWrite documentWrite;
            documentWrite.destinationPath = destination.generic_u8string();
            documentWrite.content = std::move(content);
            documentWrite.validator = [
                envelope,
                validator = std::move(validator)](
                    const std::string& path,
                    std::string& validationError)
            {
                DocumentEnvelope roundTrip;
                if (!ReadDocumentEnvelope(path, roundTrip, validationError))
                {
                    return false;
                }
                if (!SameEnvelope(envelope, roundTrip))
                {
                    validationError =
                        "The document envelope did not round-trip exactly.";
                    return false;
                }
                if (validator)
                {
                    return validator(path, validationError);
                }
                validationError.clear();
                return true;
            };
            writes.push_back(std::move(documentWrite));

            fs::path allowedRoot;
            fs::path journalDirectory;
            ResolveTransactionLocation(
                destination,
                envelope.pathHint,
                allowedRoot,
                journalDirectory);

            ProjectDocumentTransactionOptions options;
            options.transactionId = GenerateStableId();
            options.journalDirectory =
                journalDirectory.generic_u8string();
            options.allowedRoot = allowedRoot.generic_u8string();

            ProjectDocumentTransaction transaction;
            const auto result =
                transaction.Execute(std::move(writes), options);
            if (!result.success)
            {
                error = "Renegade document transaction failed [" +
                    result.code + "]: " + result.message;
                return false;
            }

            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            RemoveWithoutThrow(renderPath);
            error = std::string("Could not write Renegade document: ") +
                exception.what();
            return false;
        }
    }

    bool WriteDocumentEnvelope(
        const std::string& filePath,
        const DocumentEnvelope& envelope,
        std::string& error)
    {
        return WriteTransactionalDocument(
            filePath,
            envelope,
            true,
            {},
            {},
            error);
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
