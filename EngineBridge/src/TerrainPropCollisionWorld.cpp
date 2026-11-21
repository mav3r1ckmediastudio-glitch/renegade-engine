#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace renegade::bridge
{
    namespace
    {
        using World=TerrainPropCollisionWorld;
        // Runtime's global SceneService can be destroyed after function-local
        // statics. Keep only this weak bookkeeping alive through process exit;
        // each World still detaches and destroys its native bodies explicitly.
        std::mutex& RegistryMutex() { static auto* mutex=new std::mutex;return *mutex; }
        auto& Registry()
        {
            static auto* registry=new std::unordered_map<const wi::scene::Scene*,std::weak_ptr<World>>;
            return *registry;
        }
        bool Finite(const XMFLOAT3& v)
        { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
        bool Valid(const World::Interest& i)
        { return Finite(i.position)&&Finite(i.velocity)&&std::isfinite(i.radius)&&i.radius>=0; }
        bool TerrainKeys(const wi::terrain::Terrain& terrain,const World::Interest& interest,
            float prediction,std::size_t maximum,std::set<wi::physics::terrain_props::TerrainChunkKey>& output,
            std::string& error,std::size_t* remainingWork=nullptr)
        {
            if(!Valid(interest)||!std::isfinite(terrain.chunk_scale)||terrain.chunk_scale<=0)
            {error="Invalid terrain collision interest or chunk scale.";return false;}
            const double width=(wi::terrain::chunk_width-1)*double(terrain.chunk_scale);
            const double guard=width*.5+interest.radius;
            const double px=double(interest.position.x)+double(interest.velocity.x)*prediction;
            const double pz=double(interest.position.z)+double(interest.velocity.z)*prediction;
            double minX=std::floor((std::min(double(interest.position.x),px)-guard+width*.5)/width);
            double maxX=std::floor((std::max(double(interest.position.x),px)+guard+width*.5)/width);
            double minZ=std::floor((std::min(double(interest.position.z),pz)-guard+width*.5)/width);
            double maxZ=std::floor((std::max(double(interest.position.z),pz)+guard+width*.5)/width);
            if(!terrain.IsCenterToCamEnabled())
            {
                const double radius=std::max(0,terrain.generation);
                minX=std::max(minX,double(terrain.center_chunk.x)-radius);
                maxX=std::min(maxX,double(terrain.center_chunk.x)+radius);
                minZ=std::max(minZ,double(terrain.center_chunk.z)-radius);
                maxZ=std::min(maxZ,double(terrain.center_chunk.z)+radius);
            }
            if(minX>maxX||minZ>maxZ)return true;
            if(!std::isfinite(minX)||!std::isfinite(maxX)||!std::isfinite(minZ)||!std::isfinite(maxZ)||
                minX<INT32_MIN||minZ<INT32_MIN||maxX>INT32_MAX||maxZ>INT32_MAX||
                (maxX-minX+1)*(maxZ-minZ+1)>maximum)
            {error="Required terrain collision exceeds its bounded chunk budget.";return false;}
            for(auto x=std::int64_t(minX);x<=std::int64_t(maxX);++x)
                for(auto z=std::int64_t(minZ);z<=std::int64_t(maxZ);++z)
                {
                    if(remainingWork)
                    {
                        if(*remainingWork==0){error="Terrain collision frame query budget exceeded.";return false;}
                        --*remainingWork;
                    }
                    output.insert({std::int32_t(x),std::int32_t(z)});
                    if(output.size()>maximum)
                    {error="Required terrain collision exceeds its bounded chunk budget.";return false;}
                }
            return true;
        }
        void DiscardRecipes(std::vector<TerrainPropDocument>& documents)
        { for(auto& document:documents)document.authoring={}; }
    }

    std::shared_ptr<TerrainPropCollisionWorld> TerrainPropCollisionWorld::Create(
        wi::scene::Scene& scene,std::vector<TerrainPropDocument> documents,
        bool resident,std::string& error)
    {
        DiscardRecipes(documents);
        auto index=TerrainPropCollisionIndex::Build(documents,error);
        if(!index)return {};
        auto world=std::shared_ptr<TerrainPropCollisionWorld>(new TerrainPropCollisionWorld(scene));
        world->index_=std::move(index);world->documents_=std::move(documents);
        world->residency_=resident;
        world->stats_.indexed=world->index_->Count();
        world->stats_.sharedShapes=world->index_->ShapeCount();
        world->stats_.indexBytes=world->index_->Bytes();
        world->stats_.ready=!resident||world->index_->Count()==0;
        world->RefreshAnchors();
        std::lock_guard<std::mutex> lock(RegistryMutex());
        Registry()[&scene]=world;
        return world;
    }
    TerrainPropCollisionWorld::~TerrainPropCollisionWorld()
    { if(attached_)Detach(); }
    void TerrainPropCollisionWorld::Detach()
    {
        if(!Owner())return;
        DestroyBodies();
        wi::physics::terrain_props::ClearTerrainRequests(scene_);
        attached_=false;++generation_;stats_.ready=false;
        destinations_.clear();anchors_.clear();
        std::lock_guard<std::mutex> lock(RegistryMutex());
        auto found=Registry().find(&scene_);
        if(found!=Registry().end())
        {
            const auto current=found->second.lock();
            if(!current||current.get()==this)Registry().erase(found);
        }
    }
    std::shared_ptr<TerrainPropCollisionWorld> TerrainPropCollisionWorld::Find(
        const wi::scene::Scene& scene)
    {
        std::lock_guard<std::mutex> lock(RegistryMutex());
        auto found=Registry().find(&scene);
        auto world=found==Registry().end()?std::shared_ptr<TerrainPropCollisionWorld>{}:found->second.lock();
        // Native scene users live on their owner thread. Workers receive an
        // immutable Index snapshot, not a world/controller lifetime.
        return world&&world->Owner()?world:std::shared_ptr<TerrainPropCollisionWorld>{};
    }
    bool TerrainPropCollisionWorld::IsRegistered(const wi::scene::Scene& scene)
    {
        std::lock_guard<std::mutex> lock(RegistryMutex());
        const auto found=Registry().find(&scene);
        return found!=Registry().end()&&!found->second.expired();
    }
    bool TerrainPropCollisionWorld::Configure(Settings settings,std::string& error)
    {
        if(!Owner()||!std::isfinite(settings.requestDistance)||settings.requestDistance<0||
            !std::isfinite(settings.releaseDistance)||settings.releaseDistance<settings.requestDistance||
            !std::isfinite(settings.predictionSeconds)||settings.predictionSeconds<0||
            !std::isfinite(settings.releaseDelay)||settings.releaseDelay<0||
            settings.maximumBodies==0||settings.maximumBodies>60000||
            settings.changesPerFrame==0||settings.changesPerFrame>4096||
            settings.queryNodesPerFrame==0||settings.queryNodesPerFrame>8388608||
            settings.maximumTerrainChunks==0||settings.maximumTerrainChunks>512||
            settings.terrainChangesPerFrame==0||settings.terrainChangesPerFrame>128||
            settings.ordinaryBodyReserve>=65536)
        { error="Invalid terrain collision residency settings.";return false; }
        settings_=settings;error.clear();return true;
    }
    void TerrainPropCollisionWorld::DestroyBodies()
    {
        for(const auto& pair:bodies_) { scene_.Entity_Remove(pair.second,false);++stats_.destroyed; }
        bodies_.clear();lastWanted_.clear();stats_.resident=0;
    }
    bool TerrainPropCollisionWorld::Region(const Interest& interest,float distance,
        std::size_t maximum,std::vector<std::uint32_t>& records,std::string& error,TerrainPropQueryWork* work) const
    {
        if(!Valid(interest)) { error="Invalid terrain collision interest.";return false; }
        const auto& a=interest.position;
        XMFLOAT3 b{a.x+interest.velocity.x*settings_.predictionSeconds,
            a.y+interest.velocity.y*settings_.predictionSeconds,
            a.z+interest.velocity.z*settings_.predictionSeconds};
        const float radius=interest.radius+distance;
        TerrainPropCollisionBounds bounds{{std::min(a.x,b.x)-radius,std::min(a.y,b.y)-radius,
            std::min(a.z,b.z)-radius},{std::max(a.x,b.x)+radius,std::max(a.y,b.y)+radius,
            std::max(a.z,b.z)+radius}};
        return index_->Select(bounds,maximum,records,error,work);
    }
    bool TerrainPropCollisionWorld::SetInterest(std::uint64_t id,Interest interest,std::string& error)
    {
        if(!Owner()||id==0||!Valid(interest)||(!scripted_.count(id)&&scripted_.size()>=1024))
        { error="Invalid or excessive scripted collision interests.";return false; }
        scripted_[id]=interest;error.clear();return true;
    }
    void TerrainPropCollisionWorld::RemoveInterest(std::uint64_t id)
    { if(Owner())scripted_.erase(id); }

    bool TerrainPropCollisionWorld::Add(std::uint32_t recordIndex)
    {
        const auto& record=index_->Placement(recordIndex);
        const auto& profile=index_->GetProfile(record.profile);
        const auto entity=wi::ecs::CreateEntity();
        auto& metadata=scene_.metadatas.Create(entity);
        metadata.runtimeProjectionOnly=true;
        metadata.string_values.set("renegade.terrain_props.layer",index_->Layer(record.layer));
        metadata.string_values.set(TerrainPropPlacementMetadataKey,std::to_string(record.id));
        metadata.string_values.set(TerrainPropCollisionMetadataKey,"1");
        metadata.string_values.set(ImpactSurfaceMetadataKey,
            ImpactSurfaceTypeToken(TerrainPropSurface(profile.source.collision)));
        auto& transform=scene_.transforms.Create(entity);
        transform.RotateRollPitchYaw(XMFLOAT3{0,record.yaw,0});
        transform.Translate(record.center);transform.UpdateTransform();
        auto& body=scene_.rigidbodies.Create(entity);
        body.mass=0;body.friction=profile.source.collision.friction;
        body.shape=profile.source.collision.shape==TerrainPropShape::TrunkCapsule?body.CAPSULE:body.BOX;
        body.box.halfextents=profile.source.collision.halfExtents;
        body.capsule.radius=profile.source.collision.halfExtents.x;
        body.capsule.height=profile.source.collision.halfExtents.y;
        std::string error;
        if(!wi::physics::terrain_props::PrepareStaticBody(scene_,entity,profile.shape,
            record.scale,settings_.ordinaryBodyReserve,error))
        { scene_.Entity_Remove(entity,false);stats_.diagnostic=std::move(error);return false; }
        bodies_.emplace(recordIndex,entity);lastWanted_[recordIndex]=time_;
        ++stats_.created;
        return true;
    }

    bool TerrainPropCollisionWorld::TerrainReady(const Interest& interest,std::string& error) const
    {
        namespace native=wi::physics::terrain_props;
        for(std::size_t i=0;i<scene_.terrains.GetCount();++i)
        {
            const auto& terrain=scene_.terrains[i];
            if(!terrain.IsPhysicsEnabled())continue;
            std::set<native::TerrainChunkKey> keys;
            if(!TerrainKeys(terrain,interest,settings_.predictionSeconds,
                settings_.maximumTerrainChunks,keys,error))return false;
            for(const auto& key:keys)
            {
                const auto record=native::GetTerrainChunk(scene_,scene_.terrains.GetEntity(i),key);
                const auto* body=scene_.rigidbodies.GetComponent(record.entity);
                if(!record.valid||!body||body->IsRefreshParametersNeeded()||
                    !native::HasLiveBody(scene_,record.entity))
                {error="Destination terrain collision is not ready; source support is retained.";return false;}
            }
        }
        return true;
    }
    bool TerrainPropCollisionWorld::UpdateTerrain(const std::vector<Interest>& interests,std::string& error)
    {
        namespace native=wi::physics::terrain_props;
        std::vector<std::pair<wi::ecs::Entity,native::TerrainRequests>> requests;
        stats_.terrainRequired=stats_.terrainReady=0;
        std::size_t remainingWork=settings_.queryNodesPerFrame-
            std::min(stats_.queryNodes,settings_.queryNodesPerFrame);
        for(std::size_t i=0;i<scene_.terrains.GetCount();++i)
        {
            const auto& terrain=scene_.terrains[i];
            std::set<native::TerrainChunkKey> keys;
            if(terrain.IsPhysicsEnabled())
                for(const auto& interest:interests)
                    if(!TerrainKeys(terrain,interest,settings_.predictionSeconds,
                        settings_.maximumTerrainChunks,keys,error,&remainingWork))return false;
            stats_.terrainRequired+=keys.size();
            if(stats_.terrainRequired>settings_.maximumTerrainChunks)
            {error="Required terrain collision exceeds its bounded chunk budget.";return false;}
            requests.emplace_back(scene_.terrains.GetEntity(i),native::TerrainRequests(keys.begin(),keys.end()));
        }
        std::vector<wi::ecs::Entity> owners;
        for(const auto& item:requests)owners.push_back(item.first);
        if(owners!=terrainOwners_)
        {
            // Actual terrain addition/deletion, not a periodic inventory reset.
            native::ClearTerrainRequests(scene_);terrainOwners_=std::move(owners);
        }
        // Publish only a fully validated request set. The native terrain pump
        // generates these keys and retains source chunks alongside destinations.
        for(auto& item:requests)
            if(!native::SetTerrainRequests(scene_,item.first,std::move(item.second),error))return false;
        bool ready=true;std::size_t changes=0;
        for(const auto& item:requests)
            for(auto record:native::GetTerrainChunkRecords(scene_,item.first))
            {
                // Prediction can request a chunk again before the next terrain update.
                // A live, current heightfield already provides support on this frame.
                if (!record.valid && record.entity == wi::ecs::INVALID_ENTITY &&
                    !scene_.terrains.GetComponent(item.first)->IsGenerationBusy())
                {
                    const auto& terrain = *scene_.terrains.GetComponent(item.first);
                    wi::terrain::Chunk key;
                    key.x = record.key.x;
                    key.z = record.key.z;
                    const auto chunk = terrain.chunks.find(key);
                    if (chunk != terrain.chunks.end() && !chunk->second.invalidated)
                    {
                        const auto entity = chunk->second.entity;
                        const auto* body = scene_.rigidbodies.GetComponent(entity);
                        if (body && !body->IsRefreshParametersNeeded() &&
                            scene_.meshes.Contains(entity) && native::HasLiveBody(scene_,entity))
                        {
                            native::PublishTerrainChunk(scene_,item.first,record.key,entity,true);
                            record = {record.key,entity,true};
                        }
                    }
                }
                if(!record.valid){ready=false;continue;}
                const auto* body=scene_.rigidbodies.GetComponent(record.entity);
                if(body&&native::HasLiveBody(scene_,record.entity)&&!body->IsRefreshParametersNeeded())
                {++stats_.terrainReady;continue;}
                if(changes>=settings_.terrainChangesPerFrame){ready=false;continue;}
                ++changes;
                if(native::PrepareTerrainBody(scene_,record.entity,error))++stats_.terrainReady;
                else ready=false;
            }
        if(!ready&&error.empty())error="Preparing terrain collision; simulation is held.";
        return ready;
    }

    bool TerrainPropCollisionWorld::UpdateSceneInterests(float seconds)
    {
        if(!Owner())return false;
        if(!residency_)return Update(seconds,{});
        std::vector<Interest> interests;
        for(std::size_t i=0;i<scene_.rigidbodies.GetCount();++i)
        {
            auto& body=scene_.rigidbodies[i];
            const auto entity=scene_.rigidbodies.GetEntity(i);
            if(IsTerrainPropProjection(scene_,entity) ||
                (body.mass<=0&&!body.IsCharacterPhysics()&&!body.IsVehicle()))continue;
            const auto* transform=scene_.transforms.GetComponent(entity);
            if(!transform)continue;
            Interest interest;interest.position=transform->GetPosition();
            interest.supportOnly=!body.IsCharacterPhysics()&&!body.IsVehicle();
            if(wi::physics::terrain_props::HasLiveBody(scene_,entity))
            {
                // Teleports update Jolt before the next render transform
                // propagation. Keep destination support around that actual pose.
                interest.position=wi::physics::GetPosition(body);
                interest.velocity=wi::physics::GetVelocity(body);
            }
            // A body need not have a render object on its root. Include the
            // actual posed native bounds, with primitive dimensions covering
            // the initial pre-native frame as well.
            const auto scale=transform->GetScale();
            const float largestScale=std::max({std::abs(scale.x),std::abs(scale.y),std::abs(scale.z)});
            if(body.shape==body.BOX)
                interest.radius=largestScale*std::hypot(body.box.halfextents.x,
                    body.box.halfextents.y,body.box.halfextents.z);
            else if(body.shape==body.SPHERE)interest.radius=largestScale*body.sphere.radius;
            else if(body.shape==body.CAPSULE||body.shape==body.CYLINDER)
                interest.radius=largestScale*(body.capsule.height+body.capsule.radius);
            XMFLOAT3 minimum,maximum;
            if(wi::physics::terrain_props::BodyBounds(scene_,entity,minimum,maximum))
            {
                const auto& p=interest.position;
                const float radius=std::hypot(
                    std::max(std::abs(minimum.x-p.x),std::abs(maximum.x-p.x)),
                    std::max(std::abs(minimum.y-p.y),std::abs(maximum.y-p.y)),
                    std::max(std::abs(minimum.z-p.z),std::abs(maximum.z-p.z)));
                interest.radius=std::max(interest.radius,radius);
            }
            const auto objectIndex=scene_.objects.GetIndex(entity);
            if(objectIndex<scene_.aabb_objects.size())
                interest.radius=std::max(interest.radius,scene_.aabb_objects[objectIndex].getRadius());
            interest.radius=std::max(1.f,interest.radius);
            interests.push_back(interest);
            if(interests.size()>1024)break;
        }
        // Wicked's native CharacterComponent is independent of the JP rigid-
        // body controller; NPC interests must not depend on the camera/player.
        for(std::size_t i=0;i<scene_.characters.GetCount();++i)
        {
            const auto* transform=scene_.transforms.GetComponent(scene_.characters.GetEntity(i));
            if(!transform)continue;
            Interest interest;interest.position=transform->GetPosition();
            interest.velocity=scene_.characters[i].velocity;
            interests.push_back(interest);
            if(interests.size()>1024)break;
        }
        return Update(seconds,interests);
    }

    bool TerrainPropCollisionWorld::Update(float seconds,const std::vector<Interest>& frame)
    {
        if(!Owner())return false;
        if(!std::isfinite(seconds)||seconds<0||seconds>10)
        { stats_.ready=false;stats_.diagnostic="Invalid collision residency frame time.";return false; }
        stats_.diagnostic.clear();stats_.queryNodes=0;
        if(frame.size()>1024)
        { stats_.ready=false;stats_.diagnostic="Too many collision interests.";return false; }
        if(!residency_) { stats_.ready=true;return true; }
        time_+=seconds;
        std::vector<Interest> interests=frame;
        for(const auto& pair:scripted_)interests.push_back(pair.second);
        for(auto it=destinations_.begin();it!=destinations_.end();)
        {
            if(!scene_.transforms.Contains(it->first))it=destinations_.erase(it);
            else { interests.push_back(it->second);++it; }
        }
        std::set<std::uint32_t> required,retained;
        std::string error;
        const auto charge=[&](std::size_t work){
            stats_.queryNodes+=work;
            if(stats_.queryNodes<=settings_.queryNodesPerFrame)return true;
            stats_.ready=false;stats_.diagnostic="Terrain collision frame query budget exceeded.";
            return false;
        };
        for(const auto& interest:interests)
        {
            std::vector<std::uint32_t> records;
            TerrainPropQueryWork work;
            const bool requested=Region(interest,(interest.supportOnly?4.f:settings_.requestDistance),
                settings_.maximumBodies,records,error,&work);
            if(!charge(work.nodes))return false;
            if(!requested)
            { stats_.ready=false;stats_.diagnostic=error;return false; }
            required.insert(records.begin(),records.end());
            if(required.size()>settings_.maximumBodies)
            { stats_.ready=false;stats_.diagnostic="Required terrain collision exceeds resident capacity.";return false; }
            // Release-envelope lookup need not return a map-wide inventory.
            // If it overflows the bounded result set, retain existing bodies
            // by indexed per-body bounds rather than expanding body capacity.
            work={};
            const bool released=Region(interest,(interest.supportOnly?10.f:settings_.releaseDistance),
                settings_.maximumBodies,records,error,&work);
            if(!charge(work.nodes))return false;
            if(released)
            {
                for(auto id:records)if(bodies_.count(id))retained.insert(id);
            }
            else
            {
                auto a=interest.position;
                const auto predicted=Interest{a,interest.velocity,interest.radius+(interest.supportOnly?10.f:settings_.releaseDistance)};
                for(const auto& body:bodies_)
                {
                    if(!charge(1))return false;
                    const auto bounds=index_->Bounds(body.first);
                    const auto b=XMFLOAT3{a.x+predicted.velocity.x*settings_.predictionSeconds,
                        a.y+predicted.velocity.y*settings_.predictionSeconds,
                        a.z+predicted.velocity.z*settings_.predictionSeconds};
                    const float r=predicted.radius;
                    if(bounds.minimum.x<=std::max(a.x,b.x)+r&&bounds.maximum.x>=std::min(a.x,b.x)-r&&
                       bounds.minimum.y<=std::max(a.y,b.y)+r&&bounds.maximum.y>=std::min(a.y,b.y)-r&&
                       bounds.minimum.z<=std::max(a.z,b.z)+r&&bounds.maximum.z>=std::min(a.z,b.z)-r)
                        retained.insert(body.first);
                }
            }
        }
        stats_.required=required.size();
        std::string terrainError;
        const bool terrainReady=UpdateTerrain(interests,terrainError);
        if(!terrainReady)stats_.diagnostic=terrainError;
        std::size_t changes=0;
        const auto missing=std::count_if(required.begin(),required.end(),
            [&](auto id){return bodies_.count(id)==0;});
        bool incomplete=false;
        for(auto it=bodies_.begin();it!=bodies_.end();)
        {
            const auto id=it->first;
            const auto* body=scene_.rigidbodies.GetComponent(it->second);
            if(!body||!wi::physics::terrain_props::HasLiveBody(scene_,it->second)||body->IsRefreshParametersNeeded())
            {
                if(changes>=settings_.changesPerFrame)
                {incomplete=true;++it;continue;}
                ++changes;
                scene_.Entity_Remove(it->second,false);
                lastWanted_.erase(id);it=bodies_.erase(it);++stats_.destroyed;
                continue;
            }
            if(required.count(id)) { lastWanted_[id]=time_;++it;continue; }
            const bool pressure=bodies_.size()+missing>settings_.maximumBodies;
            if(retained.count(id)&&!pressure) { lastWanted_[id]=time_;++it;continue; }
            if((!pressure&&time_-lastWanted_[id]<settings_.releaseDelay)||changes>=settings_.changesPerFrame)
            { ++it;continue; }
            scene_.Entity_Remove(it->second,false);
            it=bodies_.erase(it);lastWanted_.erase(id);++changes;++stats_.destroyed;
        }
        bool ready=!incomplete&&terrainReady;
        for(auto id:required)
        {
            if(bodies_.count(id))continue;
            if(changes>=settings_.changesPerFrame||bodies_.size()>=settings_.maximumBodies)
            { ready=false;continue; }
            ++changes;
            if(!Add(id)) { ready=false;break; }
        }
        stats_.resident=bodies_.size();
        const auto capacity=wi::physics::terrain_props::BodyCapacity(scene_);
        stats_.nativeAllocated=capacity.allocated;stats_.nativeCapacity=capacity.maximum;
        stats_.ready=ready;
        if(ready)stats_.diagnostic.clear();
        else if(stats_.diagnostic.empty())stats_.diagnostic="Preparing required terrain collision.";
        return ready;
    }

    bool TerrainPropCollisionWorld::Destination(wi::ecs::Entity actor,const XMFLOAT3& position,
        float radius,std::string& error)
    {
        if(!Owner()||!Finite(position)||!std::isfinite(radius)||radius<0||
            !scene_.transforms.Contains(actor))
        { error="Invalid collision destination request.";return false; }
        if(!residency_) { error.clear();return true; }
        Interest destination{position,{0,0,0},radius};
        std::vector<std::uint32_t> required;
        if(!Region(destination,settings_.requestDistance,settings_.maximumBodies,required,error))return false;
        if(std::all_of(required.begin(),required.end(),[&](auto id){
            const auto found=bodies_.find(id);
            if(found==bodies_.end())return false;
            const auto* body=scene_.rigidbodies.GetComponent(found->second);
            return body&&!body->IsRefreshParametersNeeded()&&
                wi::physics::terrain_props::HasLiveBody(scene_,found->second);
        })&&TerrainReady(destination,error))
        { destinations_.erase(actor);error.clear();return true; }
        if(!destinations_.count(actor)&&destinations_.size()>=1024)
        { error="Too many pending collision destinations.";return false; }
        destinations_[actor]=destination;
        if(error.empty())error="Destination collision is not ready; source support is retained.";
        return false;
    }

    void TerrainPropCollisionWorld::CancelDestination(wi::ecs::Entity actor)
    { if(Owner())destinations_.erase(actor); }

    void TerrainPropCollisionWorld::SetRangeParts(std::vector<TerrainPropRangePart> parts)
    {
        rangeParts_=std::move(parts);
        rangeAnchors_.clear();
        for (size_t i=0; i<rangeParts_.size(); ++i) {
            const auto& p=rangeParts_[i];
            if (p.collisionAnchor) rangeAnchors_.emplace(std::make_pair(p.layer,p.placement),i);
        }
    }

    void TerrainPropCollisionWorld::RefreshAnchors()
    {
        if(!Owner())return;
        anchors_.clear();
        for(std::size_t i=0;i<scene_.metadatas.GetCount();++i)
        {
            const auto& metadata=scene_.metadatas[i];
            const auto entity=scene_.metadatas.GetEntity(i);
            if(!metadata.runtimeProjectionOnly||metadata.string_values.has("renegade.terrain_props.fine")||
                (metadata.string_values.has("renegade.terrain_props.ranged") || !scene_.objects.Contains(entity))||
                !metadata.string_values.has("renegade.terrain_props.layer")||
                !metadata.string_values.has(TerrainPropPlacementMetadataKey))continue;
            const auto text=metadata.string_values.get(TerrainPropPlacementMetadataKey);
            std::uint64_t id=0;
            auto parsed=std::from_chars(text.data(),text.data()+text.size(),id);
            if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||id==0)continue;
            anchors_.emplace(std::make_pair(metadata.string_values.get("renegade.terrain_props.layer"),id),entity);
        }
    }
    wi::ecs::Entity TerrainPropCollisionWorld::HitAnchor(const TerrainPropCollisionHit& hit) const
    {
        if(!Owner())return wi::ecs::INVALID_ENTITY;
        const auto key=std::make_pair(hit.layer,hit.placement);
        auto ranged=rangeAnchors_.find(key);
        if (ranged!=rangeAnchors_.end()) {
            const auto& part=rangeParts_[ranged->second];
            // A returned entity can be held by gameplay after this cell is evicted.
            part.EnsureIdentityAndTransform(scene_);
            scene_.metadatas.GetComponent(part.entity)->string_values.set("renegade.terrain_props.hit_anchor","1");
            return part.entity;
        }
        auto found=anchors_.find(key);
        if(found!=anchors_.end()&&scene_.transforms.Contains(found->second))return found->second;
        return wi::ecs::INVALID_ENTITY;
    }

    std::unique_ptr<TerrainPropCollisionWorld::PreparedLayer> TerrainPropCollisionWorld::Stage(
        const TerrainPropDocument& document,std::string& error)
    {
        if(!Owner()) { error="Collision edits require the scene owner thread.";return {}; }
        auto pending=std::make_unique<PreparedLayer>();
        pending->owner=shared_from_this();pending->generation=generation_;
        pending->documents=documents_;
        auto found=std::find_if(pending->documents.begin(),pending->documents.end(),
            [&](const auto& doc){return doc.layerId==document.layerId;});
        if(found==pending->documents.end())pending->documents.push_back(document);
        else *found=document;
        DiscardRecipes(pending->documents);
        pending->index=TerrainPropCollisionIndex::Build(pending->documents,error);
        if(!pending->index)return {};
        if(pending->index->Equivalent(*index_))pending->index=index_;
        return pending;
    }
    bool TerrainPropCollisionWorld::CanCommit(const PreparedLayer& pending) const
    { return Owner()&&pending.owner.lock().get()==this&&pending.generation==generation_&&pending.index!=nullptr; }
    void TerrainPropCollisionWorld::Commit(PreparedLayer& pending)
    {
        if(!CanCommit(pending))return;
        const bool changed=pending.index!=index_;
        const bool navigationChanged=pending.index->NavigationDigest()!=index_->NavigationDigest();
        if(navigationChanged)
            for(std::size_t i=0;i<scene_.characters.GetCount();++i)
            {
                auto& character=scene_.characters[i];
                character.pathquery={};
                character.SetPathGoal(character.GetPosition(),nullptr);
                character.Move(XMFLOAT3{0,0,0});
            }
        if(changed)DestroyBodies();
        index_=std::move(pending.index);documents_=std::move(pending.documents);
        if(changed)destinations_.clear();
        ++generation_;
        stats_.indexed=index_->Count();stats_.sharedShapes=index_->ShapeCount();stats_.indexBytes=index_->Bytes();
        if(changed)stats_.ready=!residency_||index_->Count()==0;
        // Changed pages can replace a visual anchor while retaining identical
        // collision geometry (for example a bush edit on the same page).
        // Refresh this inventory only at the real document commit.
        RefreshAnchors();
    }
    bool IsTerrainPropProjection(const wi::scene::Scene& scene,wi::ecs::Entity entity) noexcept
    {
        const auto* metadata=scene.metadatas.GetComponent(entity);
        return metadata&&metadata->runtimeProjectionOnly&&
            metadata->string_values.has("renegade.terrain_props.layer");
    }
    bool RequireTerrainPropDestination(wi::scene::Scene& scene,wi::ecs::Entity entity,
        const XMFLOAT3& position,float radius,std::string& error)
    {
        auto world=TerrainPropCollisionWorld::Find(scene);
        if(!world)
        {
            if(TerrainPropCollisionWorld::IsRegistered(scene))
            {error="Collision destination requires the scene owner thread.";return false;}
            error.clear();return true;
        }
        return world->Destination(entity,position,radius,error);
    }
}
