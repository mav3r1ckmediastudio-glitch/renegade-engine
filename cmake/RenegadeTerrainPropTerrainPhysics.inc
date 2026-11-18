#include <map>
#include <mutex>
#include <algorithm>

namespace wi::physics::terrain_props
{
    namespace
    {
        struct TerrainRequestState
        {
            std::shared_ptr<const TerrainRequests> requested;
            std::map<TerrainChunkKey,TerrainChunkRecord> records;
        };
        // Global Runtime scenes outlive ordinary function-local statics.
        // Controller detach clears every scene entry; only empty bookkeeping
        // storage/mutex intentionally survive until the process is reclaimed.
        std::mutex& TerrainRequestMutex(){static auto* mutex=new std::mutex;return *mutex;}
        auto& TerrainRequestRegistry()
        {
            static auto* state=new std::map<std::pair<const wi::scene::Scene*,wi::ecs::Entity>,TerrainRequestState>;
            return *state;
        }
    }
    bool SetTerrainRequests(const wi::scene::Scene& scene,wi::ecs::Entity terrain,
        TerrainRequests requested,std::string& error)
    {
        if(requested.size()>512){error="Terrain collision interest exceeds the 512-chunk budget.";return false;}
        std::sort(requested.begin(),requested.end());
        requested.erase(std::unique(requested.begin(),requested.end()),requested.end());
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        auto& state=TerrainRequestRegistry()[{&scene,terrain}];
        if(state.requested&&*state.requested==requested){error.clear();return true;}
        std::map<TerrainChunkKey,TerrainChunkRecord> next;
        for(const auto& key:requested)
        {
            const auto found=state.records.find(key);
            next.emplace(key,found==state.records.end()?TerrainChunkRecord{key}:found->second);
        }
        state.requested=std::make_shared<const TerrainRequests>(std::move(requested));
        state.records=std::move(next);error.clear();return true;
    }
    std::shared_ptr<const TerrainRequests> GetTerrainRequests(const wi::scene::Scene& scene,
        wi::ecs::Entity terrain)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        const auto found=TerrainRequestRegistry().find({&scene,terrain});
        return found==TerrainRequestRegistry().end()?std::shared_ptr<const TerrainRequests>{}:found->second.requested;
    }
    void PublishTerrainChunk(const wi::scene::Scene& scene,wi::ecs::Entity terrain,
        TerrainChunkKey key,wi::ecs::Entity chunk,bool valid)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        auto world=TerrainRequestRegistry().find({&scene,terrain});
        if(world==TerrainRequestRegistry().end())return;
        auto found=world->second.records.find(key);
        if(found==world->second.records.end())return;
        found->second={key,chunk,valid};
    }
    std::vector<TerrainChunkRecord> GetTerrainChunkRecords(const wi::scene::Scene& scene,
        wi::ecs::Entity terrain)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        std::vector<TerrainChunkRecord> result;
        const auto found=TerrainRequestRegistry().find({&scene,terrain});
        if(found==TerrainRequestRegistry().end())return result;
        result.reserve(found->second.records.size());
        for(const auto& item:found->second.records)result.push_back(item.second);
        return result;
    }
    void ClearTerrainRequests(const wi::scene::Scene& scene)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        auto& registry=TerrainRequestRegistry();
        for(auto it=registry.begin();it!=registry.end();)
            if(it->first.first==&scene)it=registry.erase(it);else ++it;
    }
    bool PrepareTerrainBody(wi::scene::Scene& scene,wi::ecs::Entity entity,std::string& error)
    {
        auto* body=scene.rigidbodies.GetComponent(entity);
        const auto* transform=scene.transforms.GetComponent(entity);
        if(!body||!transform||body->mass!=0||body->IsCharacterPhysics()||body->IsVehicle())
        {error="Terrain collision body is not ready.";return false;}
        if(HasLiveBody(scene,entity)&&!body->IsRefreshParametersNeeded()){error.clear();return true;}
        auto* mesh=scene.meshes.GetComponent(entity);
        if((body->shape==body->HEIGHTFIELD||body->shape==body->TRIANGLE_MESH)&&
            (!mesh||mesh->vertex_positions.empty()||mesh->indices.empty()))
        {error="Terrain collision mesh is pending.";return false;}
        const auto capacity=BodyCapacity(scene);
        if(capacity.allocated>=capacity.maximum)
        {error="Native capacity cannot admit destination terrain collision.";return false;}
        (void)wi::physics::GetPhysicsScene(scene);
        auto& native=wi::physics::GetRigidBody(*body);
        native.Delete();
        wi::physics::CreateRigidBodyShape(*body,transform->scale_local,mesh);
        if(native.shape==nullptr){error="Native terrain collision shape is pending.";return false;}
        wi::physics::AddRigidBody(scene,entity,*body,*transform,mesh);
        if(native.bodyID.IsInvalid()){error="Native terrain collision body allocation failed.";return false;}
        body->SetRefreshParametersNeeded(false);error.clear();return true;
    }
    TerrainChunkRecord GetTerrainChunk(const wi::scene::Scene& scene,wi::ecs::Entity terrain,TerrainChunkKey key)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        const auto found=TerrainRequestRegistry().find({&scene,terrain});
        if(found==TerrainRequestRegistry().end())return {key};
        const auto record=found->second.records.find(key);
        return record==found->second.records.end()?TerrainChunkRecord{key}:record->second;
    }
    bool HasTerrainRequests(const wi::scene::Scene& scene)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        for(const auto& item:TerrainRequestRegistry())
            if(item.first.first==&scene&&!item.second.records.empty())return true;
        return false;
    }
    bool AreTerrainRequestsReady(const wi::scene::Scene& scene)
    {
        std::lock_guard<std::mutex> lock(TerrainRequestMutex());
        for(const auto& item:TerrainRequestRegistry())
        {
            if(item.first.first!=&scene)continue;
            for(const auto& pair:item.second.records)
            {
                const auto& record=pair.second;
                const auto* body=scene.rigidbodies.GetComponent(record.entity);
                if(!record.valid||!body||body->IsRefreshParametersNeeded()||
                    !HasLiveBody(scene,record.entity))return false;
            }
        }
        return true;
    }

}
