#pragma once
#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/TerrainPropPhysicsAdapter.h"
#include <memory>

namespace renegade::bridge
{
    struct TerrainPropCollisionBounds
    {
        XMFLOAT3 minimum{0,0,0}, maximum{0,0,0};
    };
    struct TerrainPropQueryWork
    {
        std::size_t nodes = 0, shapes = 0;
    };
    struct TerrainPropCollisionHit
    {
        bool hit = false;
        StableId layer;
        std::uint64_t placement = 0;
        TerrainPropProfileId profile{};
        float fraction = 1, friction = 0.5f;
        ImpactSurfaceType surface = ImpactSurfaceType::Default;
        XMFLOAT3 position{0,0,0}, normal{0,0,0};
    };
    // Immutable, document-derived CPU data. No native render entities or body IDs.
    class TerrainPropCollisionIndex
    {
    public:
        struct Limits
        {
            std::size_t placements = 1000000;
            std::size_t bytes = 128ull*1024*1024;
            std::size_t queryNodes = 65536;
            std::size_t queryShapes = 16384;
        };
        struct Record
        {
            std::uint64_t id = 0;
            std::uint32_t profile = 0, layer = 0;
            XMFLOAT3 center{0,0,0};
            float yaw = 0, scale = 1;
        };
        struct Profile
        {
            TerrainPropProfile source;
            wi::physics::terrain_props::Shape shape;
        };
        static std::shared_ptr<const TerrainPropCollisionIndex> Build(
            const std::vector<TerrainPropDocument>& documents,
            const Limits& limits, std::string& error);
        static std::shared_ptr<const TerrainPropCollisionIndex> Build(
            const std::vector<TerrainPropDocument>& documents, std::string& error)
        { return Build(documents, Limits{}, error); }
        bool Cast(const XMFLOAT3& from, const XMFLOAT3& to, float radius,
            TerrainPropCollisionHit&, std::string& error, TerrainPropQueryWork* = nullptr) const;
        bool Select(const TerrainPropCollisionBounds&, std::size_t maximumResults,
            std::vector<std::uint32_t>&, std::string& error,
            TerrainPropQueryWork* = nullptr) const;
        const Record& Placement(std::uint32_t index) const { return records_.at(index); }
        const Profile& GetProfile(std::uint32_t index) const { return profiles_.at(index); }
        const StableId& Layer(std::uint32_t index) const { return layers_.at(index); }
        TerrainPropCollisionBounds Bounds(std::uint32_t index) const;
        std::size_t Count() const noexcept { return records_.size(); }
        std::size_t Bytes() const noexcept;
        std::size_t ShapeCount() const noexcept { return shapeCount_; }
        const std::string& NavigationDigest() const noexcept { return navigationDigest_; }
        bool Equivalent(const TerrainPropCollisionIndex&) const noexcept;
    private:
        struct Node
        {
            TerrainPropCollisionBounds bounds;
            std::uint32_t first = 0, count = 0, left = 0, right = 0;
        };
        std::uint32_t BuildNode(std::uint32_t first, std::uint32_t count);
        std::string navigationDigest_;
        Limits limits_;
        std::size_t shapeCount_ = 0;
        std::vector<Record> records_;
        std::vector<Profile> profiles_;
        std::vector<StableId> layers_;
        std::vector<std::uint32_t> order_;
        std::vector<Node> nodes_;
    };
}
