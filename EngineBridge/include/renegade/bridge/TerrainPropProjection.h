#pragma once
#include "renegade/bridge/TerrainPropPage.h"
#include <WickedEngine.h>
#include "renegade/bridge/ImpactSurface.h"
#include <map>
#include <memory>
#include <unordered_set>

namespace renegade::bridge
{
    using TerrainPropProfileId = std::array<std::uint8_t, 16>;
    enum class TerrainPropShape { None, TrunkCapsule, BoulderBox };
    struct TerrainPropCollision
    {
        TerrainPropShape shape = TerrainPropShape::None;
        XMFLOAT3 center{0, 0, 0};
        XMFLOAT3 halfExtents{1, 1, 1};
        float friction = 0.5f;
        ImpactSurfaceType surface = ImpactSurfaceType::Default;
    };
    inline ImpactSurfaceType TerrainPropSurface(const TerrainPropCollision& c) noexcept
    {
        if(c.surface!=ImpactSurfaceType::Default)return c.surface;
        return c.shape==TerrainPropShape::TrunkCapsule?ImpactSurfaceType::Wood:
            c.shape==TerrainPropShape::BoulderBox?ImpactSurfaceType::Stone:ImpactSurfaceType::Default;
    }
    // All mutation runs at the caller's scene/physics-safe point.
    class TerrainPropProjection
    {
    public:
        using Cell = std::pair<std::int32_t, std::int32_t>;
        struct PendingPages
        {
            ~PendingPages();
        private:
            friend class TerrainPropProjection;
            TerrainPropProjection* owner = nullptr;
            std::unique_ptr<wi::scene::Scene> scene;
            std::map<Cell, std::vector<wi::ecs::Entity>> entities;
            std::map<Cell, std::vector<std::uint64_t>> ids;
            std::vector<wi::ecs::Entity> installed;
            bool committed = false, installedDone = false;
        };
        explicit TerrainPropProjection(wi::scene::Scene& scene, bool createCollision = true)
            : scene_(scene), createCollision_(createCollision) {}
        ~TerrainPropProjection();
        TerrainPropProjection(const TerrainPropProjection&) = delete;
        TerrainPropProjection& operator=(const TerrainPropProjection&) = delete;
        void SetLayerIdentity(const std::string&);
        bool Register(const TerrainPropProfileId&, const wi::scene::Scene& prepared,
            TerrainPropCollision, std::string& error);
        bool Place(const TerrainPropPage&, std::string& error);
        void UnregisterUnused(const TerrainPropProfileId&);
        std::unique_ptr<PendingPages> PreparePages(const std::vector<TerrainPropPage>&,
            std::string& error);
        bool InstallPages(PendingPages&, std::string& error);
        void CommitPages(PendingPages&);
        void ReleaseOwnership() noexcept;
    private:
        struct Part { wi::ecs::Entity mesh; XMFLOAT4X4 local; };
        struct Prototype { std::vector<Part> parts; TerrainPropCollision collision; std::vector<wi::ecs::Entity> resources; };
        wi::ecs::Entity NewTransient();
        wi::scene::Scene& scene_;
        bool createCollision_ = true;
        std::string layerIdentity_;
        std::vector<wi::ecs::Entity> entities_;
        std::map<TerrainPropProfileId, Prototype> profiles_;
        std::unordered_set<std::uint64_t> placementIds_;
        std::map<Cell, std::vector<wi::ecs::Entity>> pageEntities_;
        std::map<Cell, std::vector<std::uint64_t>> pageIds_;
    };
}
