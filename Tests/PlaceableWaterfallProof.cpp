#include "../EngineBridge/src/PlaceableWaterfallEffect.h"
#include "../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include "../EngineBridge/src/WaterfallCollisionPaths.h"
#include "../WickedEngine/Editor/ModelImporter.h"
#include <Windows.h>
#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <unordered_set>
using namespace DirectX;
using namespace renegade::bridge::placeable_waterfall;
using namespace wi::graphics;
namespace fs=std::filesystem;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Settings {
    Definition waterfall;
    Settings(){waterfall.width=5.5f;}
    bool pool=false,rocks=false;float rockShift=0;
};
static wi::ecs::Entity Quad(wi::scene::Scene& scene,const char* name,
    std::initializer_list<XMFLOAT3> points,XMFLOAT4 rgba) {
    auto entity=scene.Entity_CreateMesh(name);
    auto& mesh=*scene.meshes.GetComponent(entity);
    mesh.vertex_positions.assign(points.begin(),points.end());
    mesh.vertex_uvset_0={{0,0},{1,0},{0,1},{1,1}};
    mesh.indices={0,1,2,2,1,3};
    auto& subset=mesh.subsets.emplace_back();subset.materialID=entity;subset.indexCount=6;
    mesh.SetDoubleSided(true);
    mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    mesh.CreateRenderData();
    scene.transforms.Create(entity).UpdateTransform();
    scene.objects.Create(entity).meshID=entity;
    auto& mat=scene.materials.Create(entity);
    mat.SetBaseColor(rgba);mat.SetRoughness(.94f);
    if(std::string(name).find("pool")==std::string::npos){
        auto sprite=fs::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/waterfall/rock_original.png";
        mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].name=sprite.generic_string();
        mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource=
          wi::resourcemanager::Load(mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
        Check(mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
              "Fixture original rock artwork missing");
        mat.SetBaseColor({.88f,.89f,.9f,1});
        mat.texMulAdd={5,5,0,0};
    }
    return entity;
}
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
static void AddRock(wi::scene::Scene& scene,const fs::path& file,XMFLOAT3 centre,XMFLOAT3 extent){
 wi::scene::Scene rock;ImportModel_GLTF(file.generic_string(),rock);rock.Update(0);
 Check(rock.objects.GetCount()>0,"Supplied rock failed to import");
 XMFLOAT3 lo{FLT_MAX,FLT_MAX,FLT_MAX},hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
 for(const auto& a:rock.aabb_objects){lo.x=std::min(lo.x,a._min.x);lo.y=std::min(lo.y,a._min.y);lo.z=std::min(lo.z,a._min.z);hi.x=std::max(hi.x,a._max.x);hi.y=std::max(hi.y,a._max.y);hi.z=std::max(hi.z,a._max.z);}
 auto matrix=XMMatrixTranslation(-(lo.x+hi.x)*.5f,-(lo.y+hi.y)*.5f,-(lo.z+hi.z)*.5f)*XMMatrixScaling(extent.x/(hi.x-lo.x),extent.y/(hi.y-lo.y),extent.z/(hi.z-lo.z))*XMMatrixTranslation(centre.x,centre.y,centre.z);
 std::vector<wi::ecs::Entity> roots;for(size_t i=0;i<rock.transforms.GetCount();++i)if(!rock.hierarchy.Contains(rock.transforms.GetEntity(i)))roots.push_back(rock.transforms.GetEntity(i));
 for(auto e:roots){auto* t=rock.transforms.GetComponent(e);t->MatrixTransform(matrix);t->UpdateTransform();}
 for(size_t i=0;i<rock.objects.GetCount();++i)rock.names.Create(rock.objects.GetEntity(i)).name="Collision test supplied rock";
 for(size_t i=0;i<rock.meshes.GetCount();++i)rock.meshes[i].SetBVHEnabled(true);
 scene.Merge(rock);
}
static void ApplyRockPaths(wi::scene::Scene& scene,const Definition& d,const fs::path& assets){
 // Snapshot query happens before water is added: opaque rock/floor only, no self collision.
 auto started=std::chrono::steady_clock::now();
 auto paths=TraceCollisionPaths(d,[&](XMFLOAT3 a,XMFLOAT3 b){
  XMVECTOR delta=XMLoadFloat3(&b)-XMLoadFloat3(&a);float len=XMVectorGetX(XMVector3Length(delta));if(len<.00001f)return PathHit{};
  auto hit=scene.Intersects(wi::primitive::Ray(XMLoadFloat3(&a),delta/len,0,len));
  if(hit.entity!=wi::ecs::INVALID_ENTITY&&hit.vertexID0>=0){auto* object=scene.objects.GetComponent(hit.entity);if(object){auto* mesh=scene.meshes.GetComponent(object->meshID);auto matrix=scene.ComputeEntityMatrixRecursive(hit.entity);auto a=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID0]),matrix),b=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID1]),matrix),c=XMVector3Transform(XMLoadFloat3(&mesh->vertex_positions[hit.vertexID2]),matrix);XMStoreFloat3(&hit.normal,XMVector3Normalize(XMVector3Cross(b-a,c-a)));}}
  return PathHit{hit.entity!=wi::ecs::INVALID_ENTITY,hit.position,hit.normal};
 });
 std::cout<<"COLLISION_PATH_CACHE queries="<<paths.queries<<" contacts="<<paths.contacts<<" impacts="<<paths.impacts.size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<std::endl;
 for(size_t lane=0;lane<paths.paths.size();++lane){const auto& q=paths.paths[lane].points.back();std::cout<<"PATH_END lane="<<lane<<" x="<<q.position.x<<" y="<<q.position.y<<" z="<<q.position.z<<" t="<<q.time<<std::endl;} unsigned landed=0;for(const auto& path:paths.paths)if(path.points.back().position.y<=d.landing.y+.02f)++landed;std::cout<<"COLLISION_LANDING paths="<<landed<<"/"<<paths.paths.size()<<std::endl;Check(landed==paths.paths.size(),"Collision trace exhausted before landing");Check(paths.contacts>0,"Rock test failed to contact actual geometry");
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
struct Render:wi::RenderPath3D {
    #include "WaterfallTransparentPass.inl"
    NativeShader shader;
    std::vector<wi::ecs::Entity> bodies;
    Definition definition;
    float clock=0;
    bool paused=false,fixed=false,showBody=true,collisionPaths=false;
    void Update(float dt) override{
        float step=paused?0:(fixed?1.f/60:std::min(dt,.05f));
        clock+=step;
        for(size_t i=0;i<bodies.size();++i){
            auto* mat=scene->materials.GetComponent(bodies[i]);
            if(mat){
                scene->objects.GetComponent(bodies[i])->SetRenderable(showBody&&definition.intensity>0);
                uint32_t bits=0;std::memcpy(&bits,&clock,4);
                mat->SetCustomShaderID(shader.shaderID);
                uint32_t width=0,drop=0;float h=definition.crest.y-definition.landing.y;std::memcpy(&width,&definition.width,4);std::memcpy(&drop,&h,4);mat->userdata={width,drop,static_cast<uint32_t>(i)|(collisionPaths?128u:0u)|(uint32_t(definition.turbulence*1000)<<8),bits};
                mat->SetDirty();
            }
        }
        wi::RenderPath3D::Update(step);
    }
};
int main(int argc,char** argv){
    if(argc<2){std::cerr<<"Usage: PlaceableWaterfallProof output [--height metres] [--width metres] [--dry] [--impact-spray 0..3] [--interactive] [--reopen archive]"<<std::endl;return 2;}
    try {
        Settings settings;
        std::string reopen;
        bool authoringOnly=false,interactive=false,particlesOnly=false,neutralParticles=false;
        bool impactSprayOverride=false;
        for(int i=2;i<argc;++i){
            std::string flag=argv[i];
            if(flag=="--height"&&i+1<argc)settings.waterfall.crest.y=std::stof(argv[++i]);
            else if(flag=="--width"&&i+1<argc)settings.waterfall.width=std::stof(argv[++i]);
            else if(flag=="--top-x"&&i+1<argc)settings.waterfall.crest.x=std::stof(argv[++i]);
            else if(flag=="--bottom-x"&&i+1<argc)settings.waterfall.landing.x=std::stof(argv[++i]);
            else if(flag=="--intensity"&&i+1<argc)settings.waterfall.intensity=std::stof(argv[++i]);
            else if(flag=="--spray"&&i+1<argc)settings.waterfall.spray=std::stof(argv[++i]);
            else if(flag=="--mist"&&i+1<argc)settings.waterfall.mist=std::stof(argv[++i]);
            else if(flag=="--impact-spray"&&i+1<argc){
                settings.waterfall.impactSpray=std::stof(argv[++i]);
                impactSprayOverride=true;
            }
            else if(flag=="--turbulence"&&i+1<argc)settings.waterfall.turbulence=std::stof(argv[++i]);
            else if(flag=="--rock-shift"&&i+1<argc)settings.rockShift=std::stof(argv[++i]);
            else if(flag=="--rocks")settings.rocks=true;
            else if(flag=="--pool")settings.pool=true;
            else if(flag=="--authoring-only")authoringOnly=true;
            else if(flag=="--interactive")interactive=true;
            else if(flag=="--particles-only")particlesOnly=true;
            else if(flag=="--neutral-particles")neutralParticles=true;
            else if(flag=="--reopen"&&i+1<argc)reopen=argv[++i];
        }
        auto g=BuildGeometry(settings.waterfall);
        Check(EndpointsMatch(settings.waterfall,g),"Handle mismatch");
        auto out=fs::absolute(argv[1]);fs::create_directories(out);
        HWND window=CreateWindowExW(0,L"STATIC",
            L"Renegade - G13C Impact Spray Review | use top-left slider, arrows orbit, P pause, Home reset, Esc close",
            WS_OVERLAPPEDWINDOW,80,60,1440,920,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        HWND impactPanel=nullptr,impactSlider=nullptr,impactLabel=nullptr;
        if(interactive){
            INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX),ICC_BAR_CLASSES};
            Check(InitCommonControlsEx(&controls)!=FALSE,"Impact Spray slider not available");
            ShowWindow(window,SW_SHOW);
            // Separate owned tool window: Wicked's DX12 backbuffer paints over
            // child HWNDs embedded directly in its render surface.
            // Use a plain native window class: STATIC paints its title again
            // inside the panel, overlapping the real slider label.
            WNDCLASSW panelClass{};
            panelClass.lpfnWndProc=DefWindowProcW;
            panelClass.hInstance=GetModuleHandleW(nullptr);
            panelClass.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);
            panelClass.lpszClassName=L"RenegadeWaterfallImpactPanel";
            Check(RegisterClassW(&panelClass)!=0,"Could not register Impact Spray panel");
            impactPanel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
                L"RenegadeWaterfallImpactPanel",
                L"Waterfall controls",WS_POPUP|WS_CAPTION|WS_BORDER,
                100,100,305,115,window,nullptr,GetModuleHandleW(nullptr),nullptr);
            Check(impactPanel!=nullptr,"Impact Spray controls window unavailable");
            impactLabel=CreateWindowExW(0,L"STATIC",L"Impact Spray: 1.0x",
                WS_CHILD|WS_VISIBLE|SS_LEFT,12,12,270,22,impactPanel,nullptr,
                GetModuleHandleW(nullptr),nullptr);
            impactSlider=CreateWindowExW(0,TRACKBAR_CLASSW,L"",
                WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_AUTOTICKS,8,36,275,40,
                impactPanel,nullptr,GetModuleHandleW(nullptr),nullptr);
            Check(impactLabel&&impactSlider,"Could not create native Impact Spray controls");
            ShowWindow(impactPanel,SW_SHOW);SetForegroundWindow(window);
            SendMessageW(impactSlider,TBM_SETRANGE,TRUE,MAKELPARAM(0,300));
            SendMessageW(impactSlider,TBM_SETTICFREQ,50,0);
            SendMessageW(impactSlider,TBM_SETPOS,TRUE,
                static_cast<LPARAM>(std::lround(settings.waterfall.impactSpray*100)));
            wchar_t caption[80]{};
            std::swprintf(caption,80,L"Impact Spray: %.1fx",
                          static_cast<double>(settings.waterfall.impactSpray));
            SetWindowTextW(impactLabel,caption);
        }
        struct App:wi::Application {void Initialize()override{wi::Application::Initialize();alwaysactive=true;}} app;
        app.allow_hdr=false;app.SetWindow(window);
        wi::initializer::InitializeComponentsImmediate();
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        wi::scene::Scene scene;
        auto exe=fs::u8path(wi::helper::GetExecutablePath()).parent_path();
        if(!reopen.empty()){
            wi::Archive archive(fs::absolute(reopen).generic_string(),true);
            Check(archive.IsOpen(),"Missing saved placed-waterfall fixture");
            scene.Serialize(archive);
            std::vector<AuthoredWaterfall> records;
            std::string authoringError;
            Check(EnumerateWaterfalls(scene,records,authoringError),
                  "Saved waterfall metadata failed validation");
            if(!records.empty()){
                Check(records.size()==1,"Proof fixture expected one authored waterfall");
                const float chosenImpact=settings.waterfall.impactSpray;
                settings.waterfall=records.front().settings;
                if(impactSprayOverride)settings.waterfall.impactSpray=chosenImpact;
                g=BuildGeometry(settings.waterfall);
            }
            scene.Update(0);
        }else{
            auto weather=scene.Entity_CreateTransform("Studio daylight");
            auto& w=scene.weathers.Create(weather);
            w.SetRealisticSky(false);w.ambient={.5f,.56f,.6f};w.horizon={.68f,.77f,.82f};w.zenith={.25f,.43f,.63f};
            scene.Entity_CreateLight("Sun",{14,28,-12},{1,.98f,.92f},4,180,wi::scene::LightComponent::DIRECTIONAL);
            Quad(scene,"Backdrop Cliff Face",{{-1,-.7f,-8},{-1,g.drop-.12f,-8},{-1,-.7f,8},{-1,g.drop-.12f,8}},{.31f,.34f,.36f,1});
            Quad(scene,"Rock shelf",{{-9,g.drop,-8},{.3f,g.drop,-8},{-9,g.drop,8},{.3f,g.drop,8}},{.37f,.39f,.40f,1});
            Quad(scene,"Rock floor",{{-10,-.2f,-13},{24,-.2f,-13},{-10,-.2f,13},{24,-.2f,13}},{.40f,.43f,.44f,1});
            if(settings.pool){
                float poolHalfWidth=settings.rocks?7.f:4.f;
                auto pool=Quad(scene,"Optional receiving pool (fixture only)",{{.7f,0,-poolHalfWidth},{14,0,-poolHalfWidth},{.7f,0,poolHalfWidth},{14,0,poolHalfWidth}},{.1f,.39f,.48f,.55f});
                // Native refractive pool must composite before above-surface falling water.

                auto* poolMat=scene.materials.GetComponent(pool);
                poolMat->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
                poolMat->SetBaseColor({.025f,.13f,.17f,.8f});poolMat->SetNormalMapStrength(.7f);
                poolMat->textures[wi::scene::MaterialComponent::NORMALMAP].name=(exe/"Content/waterfall/waterNormalMap.jpg").generic_string();
                poolMat->textures[wi::scene::MaterialComponent::NORMALMAP].resource=wi::resourcemanager::Load(poolMat->textures[wi::scene::MaterialComponent::NORMALMAP].name);
                poolMat->texMulAdd={6,4,0,-.01f};poolMat->SetDirty();scene.objects.GetComponent(pool)->SetRequestPlanarReflection(true);
            }
            if(settings.rocks){
                AddRock(scene,exe/"Content/waterfall/big_rock_1b.glb",{.3f,7.1f,-.6f+settings.rockShift},{5.f,2.4f,7.8f});
                AddRock(scene,exe/"Content/waterfall/big_rock_1b.glb",{2.7f,3.0f,.8f},{5.5f,2.4f,8.3f});
                AddRock(scene,exe/"Content/waterfall/big_rock_1b.glb",{4.5f,.2f,-1.7f},{3.f,1.7f,3.6f});
                scene.Update(0);ApplyRockPaths(scene,settings.waterfall,exe/"Content/waterfall");
            }else BuildEffect(scene,settings.waterfall,exe/"Content/waterfall");
            wi::ecs::Entity authoringEntity=wi::ecs::INVALID_ENTITY;
            std::string authoringError;
            Check(CreateWaterfall(scene,settings.waterfall,
                                  renegade::bridge::GenerateStableId(),
                                  authoringEntity,authoringError),
                  "Unable to persist authored waterfall anchor");
            scene.Update(0);
        }
        // A native authored marker now exists independently from visual caches.
        // Earlier G13C proof fixture archives without it remain readable.
        if(impactSlider){
            SendMessageW(impactSlider,TBM_SETPOS,TRUE,
                static_cast<LPARAM>(std::lround(settings.waterfall.impactSpray*100.f)));
            wchar_t title[70]{};
            std::swprintf(title,70,L"Impact Spray: %.1fx",
                static_cast<double>(settings.waterfall.impactSpray));
            SetWindowTextW(impactLabel,title);
        }
        // Also rebind landing emitter rates when loading a saved native fixture;
        // the authored placement slider is independent of previous spray values.
        ApplyImpactSpray(scene,settings.waterfall);
        for(size_t i=0;i<scene.materials.GetCount();++i)for(const auto& tex:scene.materials[i].textures)if(!tex.name.empty())Check(tex.resource.IsValid()&&tex.resource.GetTexture().IsValid(),"Native effect texture missing");
        // Isolated white RGB SRVs preserve authored alpha and original file resources.
        std::vector<wi::Resource> authoredParticleTextures;
        if(neutralParticles)for(size_t i=0;i<scene.emitters.GetCount();++i){
            auto entity=scene.emitters.GetEntity(i);auto* material=scene.materials.GetComponent(entity);
            auto& slot=material->textures[wi::scene::MaterialComponent::BASECOLORMAP];
            authoredParticleTextures.push_back(slot.resource);
            Texture texture=slot.resource.GetTexture();
            Swizzle swizzle;swizzle.r=swizzle.g=swizzle.b=ComponentSwizzle::ONE;swizzle.a=ComponentSwizzle::A;
            auto view=GetDevice()->CreateSubresource(&texture,SubresourceType::SRV,0,~0u,0,~0u,nullptr,nullptr,&swizzle);
            Check(view>=0,"Neutral particle texture view failed");
            wi::Resource neutral;neutral.SetTexture(texture,view);slot.resource=neutral;material->SetDirty();
        }
        Render render;render.scene=&scene;render.definition=settings.waterfall;render.fixed=!interactive;render.showBody=!particlesOnly;render.collisionPaths=settings.rocks;
        render.init(1424,881,96);render.ResizeBuffers();render.setOcclusionCullingEnabled(false);
        render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);
        wi::renderer::SetTemporalAAEnabled(false);
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(1424,881,.05f,300,XMConvertToRadians(58));
        render.camera=&camera;
        render.shader.Load(exe/"Content/waterfall");
        for(int i=0;i<1;++i){
            auto entity=scene.Entity_FindByName("Placed Waterfall Body "+std::to_string(i));
            Check(scene.objects.Contains(entity)&&scene.materials.Contains(entity),"Reopen lost placement geometry");
            render.bodies.push_back(entity);
        }
        struct Drain{wi::Application& app;~Drain(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);GetDevice()->WaitForGPU();}} drain{app};
        app.ActivatePath(&render);
        float angle=-.7f,distance=g.drop<3?6.f:20.f,eyeHeight=g.drop*.56f+1.f;
        auto view=[&](){
            XMFLOAT3 eye{1.4f+std::cos(angle)*distance,eyeHeight,std::sin(angle)*distance};
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
                XMLoadFloat3(&eye),XMVectorSet(.7f,g.drop*.50f,0,1),XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
        };view();
        auto frames=[&](int n){for(int i=0;i<n;++i)app.Run();};
        frames(240);
        unsigned particles=0;
        for(size_t i=0;i<scene.emitters.GetCount();++i)
            particles+=scene.emitters[i].GetStatistics().aliveCount;
        Check((settings.waterfall.intensity==0||(settings.waterfall.spray==0&&settings.waterfall.mist==0&&settings.waterfall.impactSpray==0))?particles==0:particles>0,"Incorrect zero/intensity VFX state");
        if(settings.waterfall.intensity==0)for(auto entity:render.bodies)Check(!scene.objects.GetComponent(entity)->IsRenderable(),"Zero-flow body remains visible");
        auto start=std::chrono::steady_clock::now();frames(90);
        auto fps=90/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"PLACEABLE_WATERFALL_GPU_PASS bodies="<<render.bodies.size()
            <<" particles="<<particles<<" fps="<<fps
            <<" top_y="<<settings.waterfall.crest.y<<" width="<<settings.waterfall.width
            <<" impactSpray="<<settings.waterfall.impactSpray
            <<" noRiverSolver=1 reopened="<<!reopen.empty()<<std::endl;
        Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/"preview.png").generic_string()),"Capture failure");
        for(int frame=0;frame<18;++frame){frames(3);char name[32];std::snprintf(name,sizeof(name),"motion-%03d.png",frame);Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/name).generic_string()),"Motion frame capture failure");}
        auto saveView=[&](const char* name,float a,float dist,float y,float targetY){
            XMFLOAT3 eye{1.4f+std::cos(a)*dist,y,std::sin(a)*dist};
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),XMVectorSet(.7f,targetY,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();frames(8);
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/name).generic_string()),"Angle capture failure");
        };
        saveView("front.png",-.12f,g.drop<3?5.f:19.f,g.drop*.55f+1,g.drop*.5f);
        saveView("opposite.png",.8f,g.drop<3?5.f:19.f,g.drop*.55f+1,g.drop*.5f);
        saveView("side.png",-1.35f,g.drop<3?5.f:19.f,g.drop*.55f+1,g.drop*.5f);
        saveView("impact.png",-.8f,g.drop<3?4.f:9.f,g.drop<3?1.6f:2.8f,g.drop<3?.4f:1.1f);
        view();frames(8);
        if(reopen.empty()){
            for(auto entity:render.bodies){
                auto* mat=scene.materials.GetComponent(entity);mat->DisableCustomShader();mat->userdata={};
            }
            if(authoringOnly){std::vector<wi::ecs::Entity> remove;
                for(size_t i=0;i<scene.names.GetCount();++i){const auto& name=scene.names[i].name;if(name.rfind("Placed Waterfall",0)==0||name.rfind("Collision water",0)==0||name.rfind("Collision landing",0)==0||name.rfind("Effect ",0)==0)remove.push_back(scene.names.GetEntity(i));}
                for(auto e:remove)scene.Entity_Remove(e);
            }
            {wi::Archive archive((out/"placeable-fixture.wiscene").generic_string(),false);
                Check(archive.IsOpen(),"Unable to save effect fixture");
                archive.SetCompressionEnabled(true);scene.Serialize(archive);}
        }
        if(interactive){
            MSG msg={};
            bool wasP=false,wasV=false;auto previousInput=std::chrono::steady_clock::now();
            while(IsWindow(window)&&!((
                  GetForegroundWindow()==window||GetForegroundWindow()==impactPanel
                  )&&(GetAsyncKeyState(VK_ESCAPE)&0x8000))){
                while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
                auto key=[&](int k){return GetForegroundWindow()==window&&(GetAsyncKeyState(k)&0x8000);};
                auto inputNow=std::chrono::steady_clock::now();float inputDelta=std::min(.05f,std::chrono::duration<float>(inputNow-previousInput).count());previousInput=inputNow;
                // Live user slider: change only landing emitters, not the core,
                // collision mesh, rock contacts, airborne spray or river.
                const float requested=static_cast<float>(
                    SendMessageW(impactSlider,TBM_GETPOS,0,0))*.01f;
                if(std::abs(requested-settings.waterfall.impactSpray)>.004f){
                    settings.waterfall.impactSpray=requested;
                    render.definition.impactSpray=requested;
                    // Keep in-memory authoring truth in sync; the scene file is
                    // saved only at the explicit native fixture save boundary.
                    std::vector<AuthoredWaterfall> authored;
                    std::string authoringError;
                    if(EnumerateWaterfalls(scene,authored,authoringError)&&authored.size()==1)
                        UpdateWaterfall(scene,authored.front().entity,
                                        settings.waterfall,authoringError);
                    ApplyImpactSpray(scene,settings.waterfall);
                    wchar_t caption[80]{};
                    std::swprintf(caption,80,L"Impact Spray: %.1fx",
                                  static_cast<double>(requested));
                    SetWindowTextW(impactLabel,caption);
                }
                if(key(VK_HOME)){angle=-.7f;distance=g.drop<3?6.f:20.f;eyeHeight=g.drop*.56f+1.f;}
                bool p=key('P');if(p&&!wasP)render.paused=!render.paused;wasP=p;bool v=key('V');if(v&&!wasV)render.showBody=!render.showBody;wasV=v;
                if(key(VK_LEFT))angle-=1.1f*inputDelta;if(key(VK_RIGHT))angle+=1.1f*inputDelta;
                if(key(VK_UP))eyeHeight+=4.f*inputDelta;if(key(VK_DOWN))eyeHeight-=4.f*inputDelta;
                if(key(VK_OEM_PLUS))distance=std::max(2.f,distance-8.f*inputDelta);
                if(key(VK_OEM_MINUS))distance=std::min(60.f,distance+8.f*inputDelta);
                view();app.Run();
            }
        }
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"PLACEABLE_WATERFALL_PROOF_FAIL "<<ex.what()<<std::endl;return 1;
    }
}
