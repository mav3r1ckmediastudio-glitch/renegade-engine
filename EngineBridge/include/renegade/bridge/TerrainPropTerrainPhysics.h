#pragma once
#include <wiScene.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Runtime interests feed the existing native terrain generation pump. This
// interface carries chunk keys/readiness only; it owns no terrain or renderer.
namespace wi::physics::terrain_props
{
    struct TerrainChunkKey
    {
        std::int32_t x=0,z=0;
        bool operator<(const TerrainChunkKey& other) const noexcept
        {return x<other.x||(x==other.x&&z<other.z);}
        bool operator==(const TerrainChunkKey& other) const noexcept {return x==other.x&&z==other.z;}
    };
    using TerrainRequests=std::vector<TerrainChunkKey>;
    struct TerrainChunkRecord
    {
        TerrainChunkKey key;
        wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;
        bool valid=false;
    };
    bool SetTerrainRequests(const wi::scene::Scene&,wi::ecs::Entity terrain,TerrainRequests,std::string& error);
    std::shared_ptr<const TerrainRequests> GetTerrainRequests(const wi::scene::Scene&,wi::ecs::Entity terrain);
    void PublishTerrainChunk(const wi::scene::Scene&,wi::ecs::Entity terrain,TerrainChunkKey,
        wi::ecs::Entity chunk,bool valid);
    std::vector<TerrainChunkRecord> GetTerrainChunkRecords(const wi::scene::Scene&,wi::ecs::Entity terrain);
    void ClearTerrainRequests(const wi::scene::Scene&);
    // Existing native HEIGHTFIELD/mesh creation, at the same owner safe point.
    bool PrepareTerrainBody(wi::scene::Scene&,wi::ecs::Entity,std::string& error);
    TerrainChunkRecord GetTerrainChunk(const wi::scene::Scene&,wi::ecs::Entity,TerrainChunkKey);
    bool HasTerrainRequests(const wi::scene::Scene&);
    bool AreTerrainRequestsReady(const wi::scene::Scene&);
}
