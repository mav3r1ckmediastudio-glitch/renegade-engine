#pragma once
#include "renegade/bridge/TerrainPropCollisionIndex.h"
#include <map>
#include "renegade/bridge/TerrainPropTerrainPhysics.h"
#include <set>
#include <thread>

namespace renegade::bridge
{
    class TerrainPropCollisionWorld : public std::enable_shared_from_this<TerrainPropCollisionWorld>
    {
    public:
        struct Settings
        {
            float requestDistance = 100, releaseDistance = 150;
            float predictionSeconds = 1, releaseDelay = 1;
            std::size_t maximumBodies = 4096, changesPerFrame = 128;
            std::size_t queryNodesPerFrame = 262144;
            std::size_t maximumTerrainChunks = 512, terrainChangesPerFrame = 16;
            std::uint32_t ordinaryBodyReserve = 4096;
        };
        struct Interest
        {
            XMFLOAT3 position{0,0,0}, velocity{0,0,0};
            float radius = 1;
            bool supportOnly = false;
        };
        struct Statistics
        {
            std::size_t indexed=0, resident=0, required=0, created=0, destroyed=0;
            std::size_t indexBytes=0, sharedShapes=0, queryNodes=0;
            std::size_t terrainRequired=0, terrainReady=0;
            std::uint32_t nativeAllocated=0, nativeCapacity=0;
            bool ready=false;
            std::string diagnostic;
        };
        struct PreparedLayer
        {
            PreparedLayer()=default;
        private:
            friend class TerrainPropCollisionWorld;
            std::weak_ptr<TerrainPropCollisionWorld> owner;
            std::uint64_t generation=0;
            std::vector<TerrainPropDocument> documents;
            std::shared_ptr<const TerrainPropCollisionIndex> index;
        };
        static std::shared_ptr<TerrainPropCollisionWorld> Create(wi::scene::Scene&,
            std::vector<TerrainPropDocument>, bool resident, std::string& error);
        ~TerrainPropCollisionWorld();
        void Detach();
        TerrainPropCollisionWorld(const TerrainPropCollisionWorld&)=delete;
        TerrainPropCollisionWorld& operator=(const TerrainPropCollisionWorld&)=delete;
        static std::shared_ptr<TerrainPropCollisionWorld> Find(const wi::scene::Scene&);
        static bool IsRegistered(const wi::scene::Scene&);
        std::shared_ptr<const TerrainPropCollisionIndex> Index() const { return index_; }
        bool Configure(Settings, std::string& error);
        // Called before the one normal native physics/render update.
        bool Update(float seconds, const std::vector<Interest>&);
        bool UpdateSceneInterests(float seconds);
        bool SetInterest(std::uint64_t id, Interest, std::string& error);
        void RemoveInterest(std::uint64_t id);
        void CancelDestination(wi::ecs::Entity actor);
        // Failed/pending destination requests retain current interests and bodies.
        bool Destination(wi::ecs::Entity actor, const XMFLOAT3&, float radius, std::string& error);
        const std::vector<TerrainPropDocument>& Documents() const { return documents_; }
        bool ResidentEnabled() const noexcept { return residency_; }
        bool Ready() const { return stats_.ready&&(!residency_||wi::physics::terrain_props::AreTerrainRequestsReady(scene_)); }
        Statistics Stats() const
        {
            auto result=stats_;
            if(Owner())
            {
                const auto capacity=wi::physics::terrain_props::BodyCapacity(scene_);
                result.nativeAllocated=capacity.allocated;result.nativeCapacity=capacity.maximum;
                result.ready=Ready();
            }
            return result;
        }
        wi::ecs::Entity HitAnchor(const TerrainPropCollisionHit&) const;
        std::unique_ptr<PreparedLayer> Stage(const TerrainPropDocument&, std::string& error);
        bool CanCommit(const PreparedLayer&) const;
        void Commit(PreparedLayer&);
        // Real content changes only; never rebuild this inventory for a point query.
        void RefreshAnchors();
    private:
        explicit TerrainPropCollisionWorld(wi::scene::Scene& scene):scene_(scene),thread_(std::this_thread::get_id()){}
        void DestroyBodies();
        bool Owner() const noexcept { return attached_&&thread_==std::this_thread::get_id(); }
        bool Add(std::uint32_t record);
        bool UpdateTerrain(const std::vector<Interest>&,std::string& error);
        bool TerrainReady(const Interest&,std::string& error) const;
        bool Region(const Interest&, float distance, std::size_t maximum,
            std::vector<std::uint32_t>&, std::string&, TerrainPropQueryWork* work=nullptr) const;
        wi::scene::Scene& scene_;
        std::thread::id thread_;
        std::vector<TerrainPropDocument> documents_;
        std::vector<wi::ecs::Entity> terrainOwners_;
        std::shared_ptr<const TerrainPropCollisionIndex> index_;
        std::map<std::uint32_t,wi::ecs::Entity> bodies_;
        std::map<std::uint32_t,double> lastWanted_;
        std::map<std::uint64_t,Interest> scripted_;
        std::map<wi::ecs::Entity,Interest> destinations_;
        std::map<std::pair<StableId,std::uint64_t>,wi::ecs::Entity> anchors_;
        Settings settings_;
        Statistics stats_;
        double time_=0;
        std::uint64_t generation_=1;
        bool residency_=false, attached_=true;
    };
    inline constexpr const char* TerrainPropPlacementMetadataKey="renegade.terrain_props.placement";
    inline constexpr const char* TerrainPropCollisionMetadataKey="renegade.terrain_props.collision";
    bool IsTerrainPropProjection(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
    bool RequireTerrainPropDestination(wi::scene::Scene&,wi::ecs::Entity,
        const XMFLOAT3&,float,std::string&);
}
