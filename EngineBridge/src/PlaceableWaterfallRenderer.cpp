#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include "PlaceableWaterfallAuthoring.h"
#include "WaterfallCollisionPaths.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
using namespace DirectX;using namespace wi::graphics;namespace fs=std::filesystem;
namespace renegade::bridge {namespace {
using namespace placeable_waterfall;
constexpr const char* GeneratedKey="renegade.waterfall.generated.v1";
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct NativeShader {
    Shader vs,ps;int shaderID=-1;
    void Load(const fs::path& root) {
        auto* dev=GetDevice();wi::vector<uint8_t> data;
        auto load=[&](const char* name,ShaderStage stage,Shader& shader){
            auto file=(root/name).generic_string();
            Check(wi::helper::FileRead(file,data),"G13 compiled effect shader not present");
            Check(dev->CreateShader(stage,data.data(),data.size(),&shader),"Failed loading placed-waterfall shader");
        };
        load("PlaceableWaterfallVS.cso",ShaderStage::VS,vs);
        load("PlaceableWaterfallPS.cso",ShaderStage::PS,ps);
        wi::renderer::CustomShader custom;
        custom.name="Renegade G13 placeable translucent waterfall (no river dependency)";
        custom.filterMask=wi::enums::FILTER_TRANSPARENT;
        PipelineStateDesc desc;
        desc.vs=&vs;desc.ps=&ps;
        desc.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
        // Translucent coverage must never write invisible pixels into scene depth.
        desc.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
        desc.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Check(dev->CreatePipelineState(&desc,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Invalid translucent effect pipeline");
        shaderID=wi::renderer::RegisterCustomShader(custom);
    }
};
static wi::ecs::Entity EmissionMesh(wi::scene::Scene& s,const Definition& d,bool falling){
    auto entity=s.Entity_CreateMesh(falling?"Effect vertical spray domain":"Effect impact strip");
    auto& mesh=*s.meshes.GetComponent(entity);
    if(falling){
        const auto g=BuildGeometry(d);
        for(size_t k=10;k<g.supports.size()-6;k+=4){
            const auto& q=g.supports[k];
            XMFLOAT3 pos{q.center.x-d.crest.x+g.forward.x*FrontSprayOffset(d),q.center.y-d.crest.y,q.center.z-d.crest.z+g.forward.z*FrontSprayOffset(d)};
            for(float sign:{-1.f,1.f})mesh.vertex_positions.push_back({
                pos.x+g.lateral.x*sign*d.width*.44f,pos.y,
                pos.z+g.lateral.z*sign*d.width*.44f
            });
        }
        for(uint32_t k=0;k+3<mesh.vertex_positions.size();k+=2)
            for(auto idx:{k,k+1,k+2,k+2,k+1,k+3})mesh.indices.push_back(idx);
    }else{
        const auto g=BuildGeometry(d);
        for(float sign:{-1.f,1.f})for(float dx:{-.85f,.85f})mesh.vertex_positions.push_back({
            g.lateral.x*sign*d.width*.48f+g.forward.x*dx*VisualScale(d),0,g.lateral.z*sign*d.width*.48f+g.forward.z*dx*VisualScale(d)
        });
        mesh.indices={0,1,2,2,1,3};
    }
    auto& subset=mesh.subsets.emplace_back();
    subset.indexCount=uint32_t(mesh.indices.size());
    mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    mesh.CreateRenderData();
    return entity;
}
static void AttachEmitter(wi::scene::Scene& scene,const char* name,
  XMFLOAT3 position,wi::ecs::Entity spawn,
  const fs::path& sprite,float count,float size,float lifetime,
  XMFLOAT3 velocity,float gravity,float opacity){
    auto entity=scene.Entity_CreateEmitter(name,position);
    auto* em=scene.emitters.GetComponent(entity);
    em->SetMaxParticleCount(5000);
    em->meshID=spawn;
    em->shaderType=wi::EmittedParticleSystem::SOFT;
    em->normal_factor=.25f;em->random_factor=1.2f;
    em->count=count;
    em->life=lifetime;em->random_life=.35f;
    em->size=size;em->scaleX=1.9f;em->scaleY=1.9f;em->rotation=.25f;
    em->velocity=velocity;em->gravity={0,gravity,0};
    em->drag=gravity>-.1f?.992f:.97f;
    em->SetOpacityCurveControl(.22f,.45f);
    em->SetCollidersDisabled(true);em->SetSorted(true);
    auto* mat=scene.materials.GetComponent(entity);
    mat->userBlendMode=wi::enums::BLENDMODE_ALPHA;mat->SetCastShadow(false);mat->SetReceiveShadow(false);
    mat->SetBaseColor({.76f,.83f,.87f,opacity});
    mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].name=sprite.generic_string();
    mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource=
        wi::resourcemanager::Load(mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
    Check(mat->textures[0].resource.IsValid(),"Missing original Renegade VFX texture");
    mat->SetDirty();
}
// This is the only group controlled by the independent Impact Spray slider.
// Count is changed on live native emitters: no geometry rebuild or shader recompile.
// Both landing effects share ONE area-sampled domain. Instead of copying
// the same spinning sprite at eight point emitters, particles appear at
// different positions and lifetimes over the actual waterline.
static void ApplyImpactSpray(wi::scene::Scene& scene,const Definition& d){
    for(const auto& source:std::array<std::pair<const char*,float>,2>{{
        {"Collision landing fine mist",235.f},{"Collision landing drift",80.f}}}){
        if(auto* emitter=scene.emitters.GetComponent(scene.Entity_FindByName(source.first)))
            emitter->count=ImpactSprayRate(d,source.second);
    }
    for(const auto& kind:std::array<std::pair<const char*,float>,2>{{
        {"Placed Waterfall - rising front impact steam",140.f},
        {"Placed Waterfall - wide front drifting mist",75.f}}}){
        if(auto* emitter=scene.emitters.GetComponent(scene.Entity_FindByName(kind.first)))
            emitter->count=ImpactSprayRate(d,kind.second);
    }
}
static wi::ecs::Entity LandingSprayDomain(wi::scene::Scene& scene,const std::vector<XMFLOAT3>& sites){
    auto entity=scene.Entity_CreateMesh("Collision landing mist source region");
    auto& mesh=*scene.meshes.GetComponent(entity);
    mesh.SetDoubleSided(true);
    for(size_t i=0;i<sites.size();++i){
        // Unequal, elliptical patches prevent repeating the same fan eight times.
        const float u=static_cast<float>(i),angle=.71f*u;
        const float width=.22f+.13f*(.5f+.5f*std::sin(u*2.31f));
        const float depth=.24f+.16f*(.5f+.5f*std::cos(u*1.71f));
        const float cs=std::cos(angle),sn=std::sin(angle);
        for(const auto& coord:std::array<XMFLOAT2,4>{{{-1,-1},{1,-1},{-1,1},{1,1}}}){
            auto x=coord.x*width,z=coord.y*depth;
            mesh.vertex_positions.push_back({
                sites[i].x+x*cs-z*sn,
                sites[i].y+.015f,
                sites[i].z+x*sn+z*cs
            });
        }
        const auto k=uint32_t(i*4);
        for(uint32_t index:{k,k+1,k+2,k+2,k+1,k+3})
            mesh.indices.push_back(index);
    }
    auto& subset=mesh.subsets.emplace_back();
    subset.indexCount=uint32_t(mesh.indices.size());
    mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    mesh.CreateRenderData();
    return entity;
}
static void BuildEffect(wi::scene::Scene& scene,const Definition& d,const fs::path& assets){
    const auto g=BuildGeometry(d);
    Check(EndpointsMatch(d,g),"Placed waterfall missed user handles");
    for(int layer=0;layer<1;++layer){
        std::string name="Placed Waterfall Body "+std::to_string(layer);
        auto entity=scene.Entity_CreateMesh(name);
        auto* mesh=scene.meshes.GetComponent(entity);
        FillSheetMesh(d,g,*mesh,entity,layer*.14f*VisualScale(d));
        mesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
        mesh->CreateRenderData();
        scene.transforms.Create(entity).UpdateTransform();
        scene.objects.Create(entity).meshID=entity;
        scene.objects.GetComponent(entity)->SetCastShadow(false);
        if(d.intensity<=0)scene.objects.GetComponent(entity)->SetRenderable(false);
        auto& mat=scene.materials.Create(entity);
        mat.userBlendMode=wi::enums::BLENDMODE_ALPHA;
        mat.SetBaseColor({.6f,.8f,.86f,.6f});
        mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].name=(assets/"AeratedWater.png").generic_string();
        mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource=wi::resourcemanager::Load(mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
        mat.SetCastShadow(false);mat.SetReceiveShadow(false);mat.SetRoughness(.62f);
        mat.SetDoubleSided(true);
        mat.userdata={0,0,static_cast<uint32_t>(layer),0};
        mat.SetDirty();
    }
    auto crest=EmissionMesh(scene,d,false);
    const float scale=VisualScale(d),front=FrontSprayOffset(d);
    auto center=d.crest;center.y+=.12f;
    auto foot=d.landing;foot.y+=.15f;
    // Horizontal spawn strips start entirely ahead of the outermost sheet.
    auto advance=[&](XMFLOAT3 p,float distance){p.x+=g.forward.x*distance;p.z+=g.forward.z*distance;return p;};
    const auto top=advance(center,.1f*scale);
    foot=advance(foot,.2f*scale);
    auto vel=[&](float forward,float up,float side=0){return XMFLOAT3{g.forward.x*forward+g.lateral.x*side,up,g.forward.z*forward+g.lateral.z*side};};
    // Fine steam-like atomised water, not individually visible sphere/droplet sprites.
    AttachEmitter(scene,"Placed Waterfall - crest vapour",top,crest,assets/"SteamMist.png",
        EffectRate(d,100)*d.spray,.65f*scale,.9f*scale,vel(1.0f,.35f),-.3f,.09f);
    AttachEmitter(scene,"Placed Waterfall - rising front impact steam",foot,crest,assets/"SteamMist.png",
        ImpactSprayRate(d,140),1.05f*scale,1.6f*scale,vel(1.2f,1.0f),.02f,.18f);
    AttachEmitter(scene,"Placed Waterfall - wide front drifting mist",foot,crest,assets/"SteamMist.png",
        ImpactSprayRate(d,75),2.f*scale,2.7f*scale,vel(1.f,.35f),.02f,.065f);
    // The same authored path supplies both the core and the particles' initial flow.
    // Dense sources intersect the core; sparse outer sources overlap and peel away.
    const auto volumes=FrontSprayVolumes(d);
    const float yaw=-std::atan2(g.forward.z,g.forward.x);
    for(size_t index=0;index<volumes.size();++index){
        const auto& volume=volumes[index];
        std::string name="Placed Waterfall - continuous breakup "+std::to_string(index);
        float rate=EffectRate(d,650)*d.spray*volume.density;
        AttachEmitter(scene,name.c_str(),volume.center,wi::ecs::INVALID_ENTITY,assets/"WaterWisp.png",
            rate,.60f*scale,.55f*scale,volume.velocity,-9.80665f,.20f);
        auto entity=scene.Entity_FindByName(name);
        auto* emitter=scene.emitters.GetComponent(entity);
        emitter->SetVolumeEnabled(true);emitter->normal_factor=.12f;emitter->random_factor=.35f;emitter->rotation=0;emitter->motionBlurAmount=.04f;
        auto* transform=scene.transforms.GetComponent(entity);transform->scale_local=volume.halfSize;
        XMStoreFloat4(&transform->rotation_local,XMQuaternionRotationRollPitchYaw(0,yaw,0));transform->SetDirty();
    }

}
static void ApplyRockPaths(wi::scene::Scene& scene,const Definition& d,const fs::path& assets,const CollisionPaths& paths){
 BuildEffect(scene,d,assets);
 auto entity=scene.Entity_FindByName("Placed Waterfall Body 0");auto* mesh=scene.meshes.GetComponent(entity);
 FillCollisionPathMesh(d,paths,*mesh,entity);mesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);mesh->CreateRenderData();
 // Replace the straight fall sources with a single source built from the traced water surface.
 std::vector<wi::ecs::Entity> obsolete;for(size_t i=0;i<scene.emitters.GetCount();++i)obsolete.push_back(scene.emitters.GetEntity(i));for(auto id:obsolete)scene.Entity_Remove(id);
 auto source=scene.Entity_CreateMesh("Collision water source cache");auto& spawn=*scene.meshes.GetComponent(source);
 FillCollisionPathMesh(d,paths,spawn,wi::ecs::INVALID_ENTITY);spawn.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);spawn.CreateRenderData();
 AttachEmitter(scene,"Collision water - entrained spray",{.08f,0,0},source,assets/"WaterWisp.png",EffectRate(d,4500)*d.spray,.55f,.38f,{.5f,-2,0},-9.80665f,.20f);
 // Spatially cluster impact sites, avoiding an emitter per ray contact.
 std::vector<XMFLOAT3> sites;
 // Existing rock contacts keep their original gentle mist; the slider does not
 // increase mist at the crest or at intermediate rock shelves.
 for(auto p:paths.impacts){
   bool occupied=false;
   for(auto q:sites)if(XMVectorGetX(XMVector3Length(XMLoadFloat3(&p)-XMLoadFloat3(&q)))<1.1f)occupied=true;
   if(!occupied&&sites.size()<16)sites.push_back(p);
 }
 for(size_t i=0;i<sites.size();++i){
    auto name="Collision water - impact "+std::to_string(i);
    AttachEmitter(scene,name.c_str(),sites[i],wi::ecs::INVALID_ENTITY,
        assets/"SteamMist.png",EffectRate(d,22)*d.mist,.50f,.45f,{.7f,.25f,0},-.2f,.075f);
 }
 // A separate, deliberately localised spray/haze burst only where falling
 // streams meet the receiving water. This preserves the approved lower strands.
 std::vector<XMFLOAT3> landings;
 for(const auto& path:paths.paths){
    auto p=path.points.back().position;
    if(p.y>d.landing.y+.08f)continue;
    p.y=d.landing.y+.10f;
    bool occupied=false;
    for(auto q:landings)
       if(XMVectorGetX(XMVector3Length(XMLoadFloat3(&p)-XMLoadFloat3(&q)))<.85f)occupied=true;
    if(!occupied&&landings.size()<12)landings.push_back(p);
 }
 Check(!landings.empty(),"No receiving-water landing spray sites");
 // A small randomised haze like the CREST vapor, not a spinning splash
 // card repeated once per stream. Subtle and diffuse even at 2x setting.
 const auto domain=LandingSprayDomain(scene,landings);
 AttachEmitter(scene,"Collision landing fine mist",{0,0,0},domain,
     assets/"SteamMist.png",ImpactSprayRate(d,235.f),
     .44f,.77f,{.14f,.86f,-.09f},-1.75f,.26f);
 if(auto* emitter=scene.emitters.GetComponent(
          scene.Entity_FindByName("Collision landing fine mist"))){
     emitter->rotation=0;
     emitter->normal_factor=0;
     emitter->random_factor=1.35f;
     emitter->random_life=.58f;
     emitter->scaleX=1.16f;
     emitter->scaleY=.83f;
     emitter->motionBlurAmount=0;
 }
 AttachEmitter(scene,"Collision landing drift",{0,0,0},domain,
     assets/"SteamMist.png",ImpactSprayRate(d,80.f),
     .68f,1.21f,{.13f,.25f,.08f},-.18f,.09f);
 if(auto* emitter=scene.emitters.GetComponent(
          scene.Entity_FindByName("Collision landing drift"))){
     emitter->rotation=0;
     emitter->normal_factor=0;
     emitter->random_factor=.78f;
     emitter->random_life=.55f;
     emitter->scaleX=1.30f;
     emitter->scaleY=.75f;
     emitter->motionBlurAmount=0;
 }

 std::cout<<"IMPACT_SPRAY_SITES count="<<landings.size()<<" multiplier="
          <<d.impactSpray<<std::endl;

}

struct Rate {wi::ecs::Entity entity;float base;int kind;};
struct Entry {AuthoredWaterfall authored;std::string geometry;std::vector<wi::ecs::Entity> entities;std::vector<Rate> rates;wi::ecs::Entity body=wi::ecs::INVALID_ENTITY;bool collision=false;};
std::string GeometryKey(Definition d){d.intensity=d.spray=d.mist=d.impactSpray=1;return Encode(d);}
}
struct PlaceableWaterfallRenderer::Impl {
 struct MeshStamp {uint64_t hash=0;size_t vertices=0,indices=0;};
 NativeShader shader;bool loaded=false;uint64_t revision=~0ull,obstacles=0;
 float clock=0,geometryScanElapsed=.35f;
 std::unordered_map<wi::ecs::Entity,MeshStamp> nearbyGeometry;
 std::map<std::string,Entry> entries;
 std::string status="Waterfall ready";
};
PlaceableWaterfallRenderer::PlaceableWaterfallRenderer():impl_(std::make_unique<Impl>()){}
PlaceableWaterfallRenderer::~PlaceableWaterfallRenderer()=default;
void RemoveGeneratedWaterfallVisuals(wi::scene::Scene& scene){std::vector<wi::ecs::Entity> ids;for(size_t i=0;i<scene.metadatas.GetCount();++i)if(scene.metadatas[i].string_values.has(GeneratedKey))ids.push_back(scene.metadatas.GetEntity(i));for(auto e:ids)scene.Entity_Remove(e);}
void PlaceableWaterfallRenderer::Clear(wi::scene::Scene& s){RemoveGeneratedWaterfallVisuals(s);impl_->entries.clear();impl_->nearbyGeometry.clear();impl_->geometryScanElapsed=.35f;impl_->obstacles=0;}
bool PlaceableWaterfallRenderer::Owns(wi::ecs::Entity e)const{return Owner(e)!=wi::ecs::INVALID_ENTITY;}
wi::ecs::Entity PlaceableWaterfallRenderer::Owner(wi::ecs::Entity e)const{for(const auto& pair:impl_->entries)for(auto id:pair.second.entities)if(id==e)return pair.second.authored.entity;return wi::ecs::INVALID_ENTITY;}
size_t PlaceableWaterfallRenderer::ActiveCount()const{return impl_->entries.size();}
const std::string& PlaceableWaterfallRenderer::Status()const{return impl_->status;}
void PlaceableWaterfallRenderer::Prepare(wi::scene::Scene& scene,uint64_t revision,float dt,bool suspended){
 auto& state=*impl_;state.clock+=std::min(dt,.05f);if(suspended)return;
 if(state.revision!=revision){Clear(scene);state.revision=revision;}
 std::vector<AuthoredWaterfall> records;std::string error;
 if(!EnumerateWaterfalls(scene,records,error)){state.status=error;return;}
 if(records.empty()){if(!state.entries.empty())Clear(scene);return;}
 try{
 auto assets=fs::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/waterfall";
 if(!state.loaded){state.shader.Load(assets);state.loaded=true;}
 // Geometry vertex counts are NOT revisions: sculpting and mesh morphs can
 // change collidable triangles while keeping every index/vertex count stable.
 // Fingerprint ONLY opaque obstacles within a waterfall's generous fall path
 // corridor and ONLY periodically, never the full 19x19 terrain every frame.
 state.geometryScanElapsed+=std::clamp(dt,0.f,.5f);
 const bool geometryScan=state.geometryScanElapsed>=.35f;
 if(geometryScan)state.geometryScanElapsed=0;
 std::unordered_set<wi::ecs::Entity> nearbyMeshes;
 auto intersectsFall=[&](const wi::primitive::AABB& aabb){
   const auto lo=aabb.getMin(),hi=aabb.getMax();
   for(const auto& record:records){
     const auto& d=record.settings;
     const float margin=std::max(10.f,d.width*1.5f+4.f);
     const auto c=d.crest,l=d.landing;
     if(hi.x>=std::min(c.x,l.x)-margin&&lo.x<=std::max(c.x,l.x)+margin&&
        hi.y>=std::min(c.y,l.y)-4.f&&lo.y<=std::max(c.y,l.y)+4.f&&
        hi.z>=std::min(c.z,l.z)-margin&&lo.z<=std::max(c.z,l.z)+margin)
       return true;
   }
   return false;
 };
 uint64_t obstacleHash=1469598103934665603ull;
 auto hashBytes=[&](const void* data,size_t count){auto* bytes=static_cast<const unsigned char*>(data);for(size_t i=0;i<count;++i){obstacleHash^=bytes[i];obstacleHash*=1099511628211ull;}};
 auto geometryHash=[](const wi::scene::MeshComponent& mesh){
   uint64_t h=1469598103934665603ull;
   auto add=[&](const void* data,size_t count){
     auto* bytes=static_cast<const unsigned char*>(data);
     for(size_t k=0;k<count;++k){h^=bytes[k];h*=1099511628211ull;}
   };
   if(!mesh.vertex_positions.empty())
     add(mesh.vertex_positions.data(),mesh.vertex_positions.size()*sizeof(XMFLOAT3));
   if(!mesh.indices.empty())
     add(mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));
   return h;
 };
 for(size_t i=0;i<scene.objects.GetCount();++i){auto entity=scene.objects.GetEntity(i);auto* md=scene.metadatas.GetComponent(entity);if(md&&md->string_values.has(GeneratedKey))continue;const auto& obj=scene.objects[i];auto* mesh=scene.meshes.GetComponent(obj.meshID);if(!mesh)continue;
 // Foreground arms and equipped view models move with the player every
 // frame. They are NOT world rocks, and must never invalidate the expensive
 // collision-guided waterfall geometry (or be hit by fall-path rays).
 if(!obj.IsRenderable()||obj.IsForeground()||
    mesh->armatureID!=wi::ecs::INVALID_ENTITY||!mesh->morph_targets.empty())
    continue;
 bool opaque=false;for(const auto& subset:mesh->subsets){auto* mat=scene.materials.GetComponent(subset.materialID);if(mat&&mat->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER&&mat->GetFilterMask()==wi::enums::FILTER_OPAQUE)opaque=true;}if(!opaque)continue;
 // A distant object must not trigger collision path rebuilds merely from
 // movement or require first-frame BVH generation. Only candidates inside
 // the padded waterfall corridor belong in the obstacle fingerprint.
 // Wicked's Scene::Update populates the object-index-aligned bounds stream.
 // Without bounds, conservatively treat the object as nearby.
 if(i>=scene.aabb_objects.size()||intersectsFall(scene.aabb_objects[i])){
   const auto meshID=scene.objects[i].meshID;
   hashBytes(&entity,sizeof(entity));hashBytes(&meshID,sizeof(meshID));
   auto matrix=scene.ComputeEntityMatrixRecursive(entity);XMFLOAT4X4 world;
   XMStoreFloat4x4(&world,matrix);hashBytes(&world,sizeof(world));
   auto n=mesh->vertex_positions.size(),indices=mesh->indices.size();
   hashBytes(&n,sizeof(n));hashBytes(&indices,sizeof(indices));
   if(!mesh->bvh.IsValid())mesh->SetBVHEnabled(true);
   nearbyMeshes.insert(meshID);
   auto stamp=state.nearbyGeometry.find(meshID);
   if(stamp==state.nearbyGeometry.end()||geometryScan||
      stamp->second.vertices!=n||stamp->second.indices!=indices){
     const auto fresh=geometryHash(*mesh);
     if(stamp!=state.nearbyGeometry.end()&&stamp->second.hash!=fresh){
       // Wicked's static BVH is not automatically rebuilt when a mesh is
       // morphed in place. Refresh collision only after change is detected.
       mesh->BuildBVH();
     }
     state.nearbyGeometry[meshID]={fresh,n,indices};
   }
   const auto h=state.nearbyGeometry.at(meshID).hash;
   hashBytes(&h,sizeof(h));
 }
 }
 if(geometryScan)
   for(auto it=state.nearbyGeometry.begin();it!=state.nearbyGeometry.end();)
     if(!nearbyMeshes.count(it->first))it=state.nearbyGeometry.erase(it);
     else ++it;
 bool obstaclesChanged=obstacleHash!=state.obstacles;state.obstacles=obstacleHash;
 if(obstaclesChanged)scene.Update(0);
 for(auto it=state.entries.begin();it!=state.entries.end();){bool exists=false;for(const auto& r:records)if(r.id==it->first)exists=true;if(!exists){for(auto e:it->second.entities)if(scene.metadatas.Contains(e))scene.Entity_Remove(e);it=state.entries.erase(it);}else++it;}
 for(const auto& record:records){auto key=GeometryKey(record.settings);auto found=state.entries.find(record.id);
 bool rebuild=found==state.entries.end()||found->second.geometry!=key||obstaclesChanged||!scene.objects.Contains(found->second.body);
 if(rebuild){
  if(found!=state.entries.end()){for(auto e:found->second.entities){auto* m=scene.metadatas.GetComponent(e);if(m&&m->string_values.has(GeneratedKey))scene.Entity_Remove(e);}state.entries.erase(found);}
  auto d=record.settings;d.intensity=d.spray=d.mist=d.impactSpray=1;
  auto paths=TraceCollisionPaths(d,[&](XMFLOAT3 a,XMFLOAT3 b){XMVECTOR delta=XMLoadFloat3(&b)-XMLoadFloat3(&a);float len=XMVectorGetX(XMVector3Length(delta));if(len<.00001f)return PathHit{};auto hit=scene.Intersects(wi::primitive::Ray(XMLoadFloat3(&a),delta/len,0,len));
   if(hit.entity!=wi::ecs::INVALID_ENTITY&&hit.vertexID0>=0){auto* object=scene.objects.GetComponent(hit.entity);if(object){auto* mesh=scene.meshes.GetComponent(object->meshID);auto matrix=scene.ComputeEntityMatrixRecursive(hit.entity);auto x=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID0]),matrix),y=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID1]),matrix),z=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID2]),matrix);XMStoreFloat3(&hit.normal,XMVector3Normalize(XMVector3Cross(y-x,z-x)));}}return PathHit{hit.entity!=wi::ecs::INVALID_ENTITY,hit.position,hit.normal};});
  wi::scene::Scene visuals;Entry entry;entry.authored=record;entry.geometry=key;entry.collision=paths.contacts>0;
  if(entry.collision){for(const auto& p:paths.paths)if(p.points.back().position.y>d.landing.y+.02f)throw std::runtime_error("Obstacle paths could not reach landing; move landing or crest");ApplyRockPaths(visuals,d,assets,paths);}else BuildEffect(visuals,d,assets);
  entry.body=visuals.Entity_FindByName("Placed Waterfall Body 0");
  for(size_t i=0;i<visuals.emitters.GetCount();++i){auto id=visuals.emitters.GetEntity(i);auto name=visuals.names.GetComponent(id)->name;int kind=name.find("wide front drifting")!=std::string::npos?3:name.find("landing")!=std::string::npos||name.find("front impact")!=std::string::npos||name.find("front drifting")!=std::string::npos?2:name.find("water - impact")!=std::string::npos?1:0;entry.rates.push_back({id,visuals.emitters[i].count,kind});}
  for(size_t i=0;i<visuals.names.GetCount();++i){auto id=visuals.names.GetEntity(i);entry.entities.push_back(id);visuals.names[i].name="__renegade_internal_waterfall_"+record.id;visuals.metadatas.Create(id).string_values.set(GeneratedKey,record.id);}
  scene.Merge(visuals);state.entries.emplace(record.id,std::move(entry));
 }
 auto& entry=state.entries.at(record.id);entry.authored=record;const auto& d=record.settings;
 for(auto rate:entry.rates)if(auto* emitter=scene.emitters.GetComponent(rate.entity))emitter->count=rate.base*d.intensity*(rate.kind==2?d.impactSpray:rate.kind==1?d.mist:rate.kind==3?d.mist*d.impactSpray:d.spray);
 auto* mat=scene.materials.GetComponent(entry.body);if(mat){uint32_t width,drop,t;float height=d.crest.y-d.landing.y;std::memcpy(&width,&d.width,4);std::memcpy(&drop,&height,4);std::memcpy(&t,&state.clock,4);mat->SetCustomShaderID(state.shader.shaderID);mat->userdata={width,drop,(entry.collision?128u:0u)|(uint32_t(d.turbulence*1000)<<8),t};mat->SetDirty();scene.objects.GetComponent(entry.body)->SetRenderable(d.intensity>0);}
 }
 state.status="Waterfall ready // "+std::to_string(state.entries.size())+" instance(s)";
 }catch(const std::exception& e){state.status=std::string("WATERFALL ERROR // ")+e.what();}
}
}
