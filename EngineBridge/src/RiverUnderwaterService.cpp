#include "renegade/bridge/RiverUnderwaterService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/IdentityService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <unordered_map>

namespace renegade::bridge
{
    namespace
    {
        constexpr std::size_t MaxPrisms = 16384;
        constexpr const char* PassName = "Renegade River Underwater";
        bool Finite(const XMFLOAT3& p)
        { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
        float Dot(const XMFLOAT4& p,const XMFLOAT3& v)
        { return p.x*v.x+p.y*v.y+p.z*v.z; }
        XMFLOAT4 Plane(const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& c)
        {
            XMFLOAT4 p;
            auto n=DirectX::XMVector3Normalize(DirectX::XMVector3Cross(
                DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&b),DirectX::XMLoadFloat3(&a)),
                DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&c),DirectX::XMLoadFloat3(&a))));
            DirectX::XMStoreFloat4(&p,n);p.w=-Dot(p,a);return p;
        }
        struct Node { XMFLOAT4 minimum{},maximum{}; };
        struct PackedPrism { std::array<XMFLOAT4,5> planes; XMFLOAT4 scattering,extinction; };
        static_assert(sizeof(Node)==32 && sizeof(PackedPrism)==112);
        // Version 1 is emitted by RiverTerrainLayerService with sorted JSON keys:
        // chunks, layers, version. Read only its small suffix, never copy/parse
        // the potentially 32 MB sculpt baseline on each rendering frame.
        std::unordered_map<std::string,float> LinkedDepths(const wi::scene::Scene& scene)
        {
            std::unordered_map<std::string,float> depths;
            for(std::size_t i=0;i<scene.terrains.GetCount();++i)
            {
                auto* metadata=scene.metadatas.GetComponent(scene.terrains.GetEntity(i));
                if(!metadata || !metadata->string_values.has(RiverTerrainLayersMetadataKey))continue;
                const auto& text=metadata->string_values.get(RiverTerrainLayersMetadataKey);
                if(text.size()>32*1024*1024)continue;
                const auto start=text.rfind("\"layers\":");
                if(start==std::string::npos || text.size()-start>16384)continue;
                try
                {
                    auto doc=nlohmann::json::parse("{"+text.substr(start));
                    if(doc.at("version").get<int>()!=1 || !doc.at("layers").is_array() || doc.at("layers").size()>32)continue;
                    for(const auto& layer:doc.at("layers"))
                    {
                        auto id=layer.at("river").get<std::string>();
                        const float depth=layer.at("depth").get<float>();
                        if(IsValidStableId(id)&&std::isfinite(depth)&&depth>=.5f&&depth<=5)
                            depths[id]=std::max(depths[id],depth);
                    }
                }
                catch(const std::exception&) { continue; }
            }
            return depths;
        }
    }

    bool MakeRiverWaterPrism(const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& c,
        float depth,const XMFLOAT4& color,const XMFLOAT3& extinction,RiverWaterPrism& result)
    {
        result={};
        if(!Finite(a)||!Finite(b)||!Finite(c)||!Finite(extinction)||!std::isfinite(depth)||depth<.05f||depth>5)return false;
        auto top=Plane(a,b,c);
        // Falling sheets/vertical walls are not pools. Also rejects degenerate triangles.
        if(!std::isfinite(top.y)||std::abs(top.y)<.15f)return false;
        if(top.y<0) {top.x=-top.x;top.y=-top.y;top.z=-top.z;top.w=-top.w;}
        result.planes[0]=top;
        result.planes[1]={-top.x,-top.y,-top.z,-top.w-top.y*depth};
        const XMFLOAT3 centre{(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3-depth*.5f,(a.z+b.z+c.z)/3};
        const XMFLOAT3 vertices[3]={a,b,c};
        for(int i=0;i<3;++i)
        {
            const auto& v=vertices[i];const auto& next=vertices[(i+1)%3];
            XMFLOAT3 low{v.x,v.y-depth,v.z};auto side=Plane(v,next,low);
            if(!std::isfinite(side.x)||!std::isfinite(side.z))return false;
            if(Dot(side,centre)+side.w>0){side.x=-side.x;side.y=-side.y;side.z=-side.z;side.w=-side.w;}
            result.planes[i+2]=side;
        }
        result.minimum={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y})-depth,std::min({a.z,b.z,c.z})};
        result.maximum={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
        const float density=std::isfinite(color.w)?std::clamp(color.w,.01f,4.f):.6f;
        result.scattering={std::clamp(color.x,0.f,1.f),std::clamp(color.y,0.f,1.f),std::clamp(color.z,0.f,1.f),density};
        result.extinction={std::clamp(extinction.x,0.f,10.f)*density,
            std::clamp(extinction.y,0.f,10.f)*density,std::clamp(extinction.z,0.f,10.f)*density,0};
        return true;
    }
    bool IntersectRiverWaterPrism(const RiverWaterPrism& prism,const XMFLOAT3& origin,
        const XMFLOAT3& direction,float maximumDistance,float& entry,float& exit)
    {
        entry=0;exit=maximumDistance;
        if(!Finite(origin)||!Finite(direction)||!std::isfinite(maximumDistance)||maximumDistance<=0)return false;
        for(const auto& plane:prism.planes)
        {
            const float distance=Dot(plane,origin)+plane.w,denominator=Dot(plane,direction);
            if(std::abs(denominator)<1e-7f){if(distance>1e-5f)return false;continue;}
            const float t=-distance/denominator;
            if(denominator<0)entry=std::max(entry,t);else exit=std::min(exit,t);
            if(entry>exit)return false;
        }
        return exit>entry;
    }
    std::vector<RiverWaterPrism> CaptureRiverWaterPrisms(const wi::scene::Scene& scene)
    {
        std::vector<RiverWaterPrism> prisms;
        const auto depths=LinkedDepths(scene);
        if(depths.empty())return prisms;
        for(std::size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto entity=scene.splines.GetEntity(i);
            const auto found=depths.find(PersistentEntityId(scene,entity));
            if(found==depths.end())continue;
            const auto* material=scene.materials.GetComponent(entity);
            const auto* object=scene.objects.GetComponent(entity);
            const auto* mesh=scene.meshes.GetComponent(object?object->meshID:entity);
            if(!material||material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER||!mesh||!object||!object->IsRenderable())continue;
            const auto matrix=scene.ComputeEntityMatrixRecursive(entity);
            for(std::size_t j=0;j+2<mesh->indices.size();j+=3)
            {
                XMFLOAT3 v[3];bool valid=true;
                for(int k=0;k<3;++k)
                {
                    const auto index=mesh->indices[j+k];if(index>=mesh->vertex_positions.size()){valid=false;break;}
                    DirectX::XMStoreFloat3(&v[k],DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&mesh->vertex_positions[index]),matrix));
                }
                RiverWaterPrism prism;
                if(valid&&MakeRiverWaterPrism(v[0],v[1],v[2],found->second,material->baseColor,XMFLOAT3(material->sheenColor.x,material->sheenColor.y,material->sheenColor.z),prism))
                {
                    if(prisms.size()>=MaxPrisms)return {}; // never silently truncate a water volume
                    prisms.push_back(prism);
                }
            }
        }
        return prisms;
    }

    void RiverUnderwaterRenderer::Update(wi::RenderPath3D& path)
    {
        auto& passes=path.custom_post_processes;
        passes.erase(std::remove_if(passes.begin(),passes.end(),[](const auto& p){return p.name==PassName;}),passes.end());
        auto* device=wi::graphics::GetDevice();
        if(!device||!path.scene||!path.camera)return;
        // The shader leaves ocean-immersed pixels to the preceding native ocean pass.
        auto captured=CaptureRiverWaterPrisms(*path.scene);
        std::uint64_t hash=1469598103934665603ull;
        for(const auto& prism:captured)
            for(std::size_t i=0;i<sizeof(prism);++i){hash^=reinterpret_cast<const unsigned char*>(&prism)[i];hash*=1099511628211ull;}
        if(hash!=fingerprint_)
        {
            prisms_=std::move(captured);fingerprint_=hash;buffer_={};nodeCount_=0;
            if(!prisms_.empty())
            {
                std::vector<Node> nodes;
                std::vector<PackedPrism> packed;
                std::vector<std::size_t> order(prisms_.size());
                for(std::size_t i=0;i<order.size();++i)order[i]=i;
                std::function<void(std::size_t,std::size_t)> build=[&](std::size_t first,std::size_t last)
                {
                    const auto index=nodes.size();nodes.emplace_back();
                    XMFLOAT3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
                    for(auto i=first;i<last;++i)
                    {
                        const auto& p=prisms_[order[i]];
                        low.x=std::min(low.x,p.minimum.x);low.y=std::min(low.y,p.minimum.y);low.z=std::min(low.z,p.minimum.z);
                        high.x=std::max(high.x,p.maximum.x);high.y=std::max(high.y,p.maximum.y);high.z=std::max(high.z,p.maximum.z);
                    }
                    nodes[index].minimum={low.x,low.y,low.z,0};nodes[index].maximum={high.x,high.y,high.z,0};
                    if(last-first==1)
                    {
                        const auto& p=prisms_[order[first]];
                        nodes[index].minimum.w=float(packed.size()+1);
                        packed.push_back({p.planes,p.scattering,p.extinction});
                    }
                    else
                    {
                        const float dx=high.x-low.x,dy=high.y-low.y,dz=high.z-low.z;
                        const int axis=dx>=dy&&dx>=dz?0:(dy>=dz?1:2);
                        const auto mid=first+(last-first)/2;
                        auto centre=[&](std::size_t i){const auto& p=prisms_[i];return axis==0?p.minimum.x+p.maximum.x:(axis==1?p.minimum.y+p.maximum.y:p.minimum.z+p.maximum.z);};
                        std::nth_element(order.begin()+first,order.begin()+mid,order.begin()+last,[&](auto a,auto b){return centre(a)<centre(b);});
                        build(first,mid);build(mid,last);
                    }
                    nodes[index].maximum.w=float(nodes.size()); // threaded tree escape, no GPU stack
                };
                build(0,order.size());nodeCount_=std::uint32_t(nodes.size());
                std::vector<unsigned char> bytes(nodes.size()*sizeof(Node)+packed.size()*sizeof(PackedPrism));
                std::memcpy(bytes.data(),nodes.data(),nodes.size()*sizeof(Node));
                std::memcpy(bytes.data()+nodes.size()*sizeof(Node),packed.data(),packed.size()*sizeof(PackedPrism));
                wi::graphics::GPUBufferDesc desc;desc.size=bytes.size();
                desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;desc.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;
                if(!device->CreateBuffer(&desc,bytes.data(),&buffer_))error_="Could not upload river underwater volumes";
            }
        }
        if(!buffer_.IsValid()||prisms_.empty())return;
        // Only dispatch near an immersed camera/waterline. Each pixel still
        // tests its own near-plane location, giving a partial-submersion edge.
        XMFLOAT3 eye;DirectX::XMStoreFloat3(&eye,path.camera->GetEye());
        float margin=.25f;
        for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)
        {
            auto p=DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(float(x),float(y),1,1),path.camera->GetInvViewProjection());
            margin=std::max(margin,DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(p,DirectX::XMLoadFloat3(&eye)))));
        }
        bool nearby=false;
        for(const auto& p:prisms_)
            if(eye.x>=p.minimum.x-margin&&eye.x<=p.maximum.x+margin&&eye.y>=p.minimum.y-margin&&eye.y<=p.maximum.y+margin&&eye.z>=p.minimum.z-margin&&eye.z<=p.maximum.z+margin){nearby=true;break;}
        if(!nearby)return;
        // Fly cameras below solid terrain must not see an underwater treatment.
        // Native scene query is read-only; depth reconstruction clips visible rays.
        auto ground=wi::scene::Pick(wi::primitive::Ray(XMFLOAT3(eye.x,eye.y+1000,eye.z),XMFLOAT3(0,-1,0)),wi::enums::FILTER_TERRAIN,~0u,*path.scene);
        if(ground.entity!=wi::ecs::INVALID_ENTITY && ground.position.y>eye.y+.02f)return;
        if(!shaderAttempted_)
        {
            shaderAttempted_=true;
            const auto shaderPath=std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content"/"river"/"RenegadeRiverUnderwaterCS.cso";
            wi::vector<std::uint8_t> bytes;
            if(device->GetShaderFormat()!=wi::graphics::ShaderFormat::HLSL6 ||
                !wi::helper::FileRead(shaderPath.generic_u8string(),bytes) ||
                !device->CreateShader(wi::graphics::ShaderStage::CS,bytes.data(),bytes.size(),&shader_))
            {
                error_="River underwater DX12 shader is missing or incompatible";
                wi::backlog::post(error_,wi::backlog::LogLevel::Error);return;
            }
        }
        if(!shader_.IsValid())return;
        wi::RenderPath3D::CustomPostprocess pass;
        pass.name=PassName;pass.computeshader=shader_;
        pass.stage=wi::RenderPath3D::CustomPostprocess::Stage::BeforeTonemap;
        pass.params0={float(device->GetDescriptorIndex(&buffer_,wi::graphics::SubresourceType::SRV)),float(nodeCount_),60,0};
        pass.params1={path.scene->weather.IsOceanEnabled()?1.f:0.f,0,0,0};passes.push_back(std::move(pass));
    }
}
