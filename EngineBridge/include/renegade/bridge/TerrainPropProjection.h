#pragma once
#include "renegade/bridge/TerrainPropPage.h"
#include <WickedEngine.h>
#include <map>
#include <unordered_set>

namespace renegade::bridge
{
    using TerrainPropProfileId = std::array<std::uint8_t, 16>;
    enum class TerrainPropShape { None, TrunkCapsule, BoulderBox };
    struct TerrainPropCollision
    {
        TerrainPropShape shape = TerrainPropShape::None;
        XMFLOAT3 center{0, 0, 0};
        XMFLOAT3 halfExtents{1, 1, 1}; // Capsule uses x for radius, y for cylindrical half-height.
    };

    // Scene-thread only. Destroy before its owning scene. Prepared scenes
    // supply static model geometry in model-local coordinates.
    class TerrainPropProjection
    {
    public:
        explicit TerrainPropProjection(wi::scene::Scene& scene) : scene_(scene) {}
        ~TerrainPropProjection();
        TerrainPropProjection(const TerrainPropProjection&) = delete;
        TerrainPropProjection& operator=(const TerrainPropProjection&) = delete;
        bool Register(const TerrainPropProfileId&, const wi::scene::Scene& prepared,
            TerrainPropCollision, std::string& error);
        bool Place(const TerrainPropPage&, std::string& error);
    private:
        struct Part { wi::ecs::Entity mesh; XMFLOAT4X4 local; };
        struct Prototype { std::vector<Part> parts; TerrainPropCollision collision; };
        wi::ecs::Entity NewTransient();
        wi::scene::Scene& scene_;
        std::vector<wi::ecs::Entity> entities_;
        std::map<TerrainPropProfileId, Prototype> profiles_;
        std::unordered_set<std::uint64_t> placementIds_;
    };
}
