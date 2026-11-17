#pragma once
#include "renegade/bridge/TerrainPropPage.h"
#include <functional>

namespace renegade::bridge
{
    struct TerrainPropMixEntry
    {
        std::array<std::uint8_t,16> profileId{};
        float densityPerSquareMetre = .02f;
        float minSpacing = 2, minScale = 1, maxScale = 1;
    };
    struct TerrainPropMix { std::string name; std::vector<TerrainPropMixEntry> entries; };
    struct TerrainPropRecipe
    {
        std::uint64_t seed = 1;
        float cellSize = 32, candidateStep = 1;
        TerrainPropMix mix;
    };
    struct TerrainPropClearing { float x = 0, z = 0, radius = 1; };
    struct TerrainPropCandidateBinding { std::uint64_t candidateKey = 0, placementId = 0; };
    struct TerrainPropOverride
    {
        std::uint64_t candidateKey = 0, placementId = 0;
        std::array<std::uint8_t,16> profileId{};
        std::array<float,3> position{};
        float yawRadians = 0, scale = 1;
    };
    struct TerrainPropAuthoringState
    {
        TerrainPropRecipe recipe;
        std::vector<TerrainPropMix> savedMixes;
        std::vector<TerrainPropCandidateBinding> bindings;
        std::vector<std::uint64_t> tombstones;
        std::vector<TerrainPropClearing> clearings;
        std::vector<TerrainPropOverride> overrides;
    };
    struct TerrainPropBounds { float minX = 0, minZ = 0, maxX = 0, maxZ = 0; };
    enum class TerrainPropEditOperation { Generate, Paint, Erase, Reproject };
    struct TerrainPropEditRequest
    {
        TerrainPropEditOperation operation = TerrainPropEditOperation::Generate;
        TerrainPropBounds bounds;
        std::vector<std::array<float,3>> path;
        float radius = 4, sampleDistance = 1;
        std::string expectedRevision, currentRevision;
        // Return false to cancel. Staging never changes caller state.
        std::function<bool(float)> progress;
    };
    struct TerrainPropPageDelta
    {
        std::int32_t cellX = 0, cellZ = 0;
        bool hadBefore = false, hasAfter = false;
        TerrainPropPage before, after;
    };
    struct TerrainPropEditWork
    {
        // Transient diagnostics; these counters are never serialized.
        std::uint64_t discIndexEntries = 0, discBucketLookups = 0, discTests = 0;
    };
    struct TerrainPropEdit
    {
        TerrainPropEditWork work;
        TerrainPropAuthoringState authoring;
        std::vector<TerrainPropPage> pages;
        std::vector<TerrainPropPageDelta> changedPages;
        TerrainPropBounds changedBounds;
        std::uint64_t nextPlacementId = 1;
    };
    // Sampler returns false outside finite terrain; never extrapolate a height.
    using TerrainPropHeightSampler = std::function<bool(float,float,float&)>;
    bool ValidateTerrainPropAuthoring(const TerrainPropAuthoringState&, std::string& error);
    bool SampleTerrainPropStroke(const std::vector<std::array<float,3>>& path,
        float distance, std::vector<std::array<float,3>>& samples, std::string& error);
    bool BuildTerrainPropEdit(const std::vector<TerrainPropPage>& pages,
        const TerrainPropAuthoringState& authoring, std::uint64_t nextPlacementId,
        const TerrainPropEditRequest&, const TerrainPropHeightSampler&,
        TerrainPropEdit& result, std::string& error);
}
