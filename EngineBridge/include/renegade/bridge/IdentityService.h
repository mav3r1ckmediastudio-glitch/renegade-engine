#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    using StableId = std::string;

    inline constexpr const char* PersistentEntityIdMetadataKey =
        "renegade.persistent_entity_id";

    [[nodiscard]] bool IsValidStableId(const StableId& value) noexcept;
    [[nodiscard]] StableId GenerateStableId();

    struct DocumentEnvelope
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = "renegade-document";
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId documentId;
        StableId projectId;
        std::string documentType;
        std::string pathHint;
        std::string generatorVersion;
        std::uint32_t migratedFromVersion = 0;
    };

    [[nodiscard]] DocumentEnvelope CreateDocumentEnvelope(
        const StableId& projectId,
        std::string documentType,
        std::string pathHint,
        std::string generatorVersion);

    [[nodiscard]] bool ValidateDocumentEnvelope(
        const DocumentEnvelope& envelope,
        std::string& error);
    [[nodiscard]] bool WriteDocumentEnvelope(
        const std::string& filePath,
        const DocumentEnvelope& envelope,
        std::string& error);
    [[nodiscard]] bool ReadDocumentEnvelope(
        const std::string& filePath,
        DocumentEnvelope& envelope,
        std::string& error);
    [[nodiscard]] bool RetargetDocumentEnvelope(
        DocumentEnvelope& envelope,
        const std::string& pathHint,
        std::string& error);
    [[nodiscard]] bool ValidateDocumentEnvelopes(
        const StableId& expectedProjectId,
        const std::vector<DocumentEnvelope>& envelopes,
        std::string& error);

    enum class EntityIdentityIssueCode
    {
        Missing,
        Malformed,
        Duplicate,
    };

    struct EntityIdentityIssue
    {
        EntityIdentityIssueCode code = EntityIdentityIssueCode::Missing;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity conflictingEntity = wi::ecs::INVALID_ENTITY;
        StableId value;
        std::string message;
    };

    struct EntityIdentityValidation
    {
        std::vector<EntityIdentityIssue> issues;

        [[nodiscard]] bool IsValid() const noexcept
        {
            return issues.empty();
        }

        [[nodiscard]] std::string Summary() const;
    };

    [[nodiscard]] std::vector<wi::ecs::Entity>
        EnumeratePersistentSceneEntities(const wi::scene::Scene& scene);
    [[nodiscard]] StableId PersistentEntityId(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity);
    [[nodiscard]] bool AssignPersistentEntityId(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const StableId& id,
        std::string& error);
    [[nodiscard]] bool AssignNewPersistentEntityId(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        std::string& error);
    [[nodiscard]] EntityIdentityValidation ValidatePersistentEntityIdentities(
        const wi::scene::Scene& scene);
    [[nodiscard]] bool EnsurePersistentEntityIdentities(
        wi::scene::Scene& scene,
        std::string& error);

    class EntityIdentityIndex
    {
    public:
        [[nodiscard]] bool Build(
            const wi::scene::Scene& scene,
            std::string& error);
        [[nodiscard]] wi::ecs::Entity Resolve(
            const StableId& id) const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept
        {
            return entitiesById_.size();
        }

    private:
        std::unordered_map<StableId, wi::ecs::Entity> entitiesById_;
    };
}
