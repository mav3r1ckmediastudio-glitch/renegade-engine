#pragma once
#include "renegade/bridge/TerrainPropProjection.h"
#include "renegade/bridge/TerrainPropAuthoring.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>

namespace renegade::bridge
{
    struct TerrainPropProfile
    {
        TerrainPropProfileId id{};
        StableId modelAssetId;
        TerrainPropCollision collision;
    };

    struct TerrainPropDocument
    {
        StableId layerId;
        std::uint64_t nextPlacementId = 1;
        // Returns zero when the counter is exhausted. Deletion never decrements it.
        std::uint64_t AllocatePlacementId() noexcept
        {
            if (!nextPlacementId || nextPlacementId == UINT64_MAX) return 0;
            return nextPlacementId++;
        }
        std::vector<TerrainPropProfile> profiles;
        std::vector<TerrainPropPage> pages;
        TerrainPropAuthoringState authoring;
    };

    // Compare authored content for history rebasing; high-water mark intentionally excluded.
    bool TerrainPropDocumentContentEqual(const TerrainPropDocument&, const TerrainPropDocument&);
    bool ComputeTerrainPropDocumentContentDigest(const TerrainPropDocument&,
        std::string& digest, std::string& error);

    // Write immutable products before publishing their reference in authored
    // metadata. Existing WISCENE save/backup/rollback then selects one revision.
    bool PublishTerrainPropRevision(const std::filesystem::path& projectRoot,
        const StableId& projectId, const TerrainPropDocument& document,
        wi::scene::Scene& scene, wi::ecs::Entity layerOwner, std::string& error,
        ProjectDocumentTransactionOptions options = {});

    bool ReadTerrainPropRevision(const std::filesystem::path& projectRoot,
        const StableId& projectId, const std::string& digest,
        TerrainPropDocument& document, std::string& error);

    bool ValidateTerrainPropRevisionLinks(const wi::scene::Scene& scene,
        const std::filesystem::path& scenePath, std::string& error);

    // Called on detached candidate scenes before Studio or Runtime adopts them.
    bool RestoreTerrainPropRevisions(wi::scene::Scene& candidate,
        const std::filesystem::path& scenePath, std::string& error,
        std::vector<TerrainPropDocument>* documents = nullptr, bool createCollision = true);
}
