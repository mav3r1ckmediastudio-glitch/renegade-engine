#pragma once
#include "renegade/bridge/ProjectileSimulation.h"
#include <wiScene.h>
#include <string>
#include <algorithm>
#include <array>
#include <cmath>

namespace renegade::runtime
{
    struct RuntimeImpactMarks
    {
        using Surface=bridge::ImpactSurfaceType;
        static std::string Key(Surface surface)
        {
            const char* name=surface==Surface::Metal?"metal":surface==Surface::Wood?"wood":
                surface==Surface::Concrete?"concrete":surface==Surface::Stone?"stone":
                surface==Surface::Glass?"glass":surface==Surface::Dirt?"dirt":
                surface==Surface::Character?"skin":"default";
            return std::string("renegade.impact.mark.")+name;
        }
        static wi::ecs::Entity FindMaterial(const wi::scene::Scene& scene,Surface surface)
        {
            const auto key=Key(surface);
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas[i].bool_values.has(key) && scene.metadatas[i].bool_values.get(key)) {
                    const auto entity=scene.metadatas.GetEntity(i);
                    const auto* m=scene.materials.GetComponent(entity);
                    if(m && m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
                       m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid())
                        return entity;
                }
            return wi::ecs::INVALID_ENTITY;
        }
        static const wi::graphics::Texture& GlassPuncture()
        {
            static wi::graphics::Texture texture;auto* device=wi::graphics::GetDevice();
            if(texture.IsValid() || !device)return texture;
            constexpr unsigned width=64;std::array<unsigned char,width*width*4> pixels{};
            for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x) {
                const float u=(float(x)+.5f)/32-1,v=(float(y)+.5f)/32-1;
                const float angle=std::atan2(v,u),radius=std::sqrt(u*u+v*v);
                const float edge=.82f+.055f*std::sin(angle*7)+.025f*std::sin(angle*13);
                const float r=radius/edge;
                const float alpha=std::clamp((1-r)*16,0.f,1.f);
                const float rim=std::clamp((r-.52f)*5,0.f,1.f);
                const auto shade=static_cast<unsigned char>((.035f+rim*(.47f+.10f*std::sin(angle*9)))*255);
                const unsigned i=(y*width+x)*4;
                pixels[i]=pixels[i+1]=pixels[i+2]=shade;pixels[i+3]=static_cast<unsigned char>(alpha*255);
            }
            wi::graphics::TextureDesc desc;desc.width=desc.height=width;
            desc.format=wi::graphics::Format::R8G8B8A8_UNORM;desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;data.data_ptr=pixels.data();data.row_pitch=width*4;data.slice_pitch=pixels.size();
            device->CreateTexture(&desc,&data,&texture);return texture;
        }
        static const wi::graphics::Texture& BloodTrickle(unsigned variant)
        {
            static std::array<wi::graphics::Texture,4> textures;auto& texture=textures[variant%4];
            auto* device=wi::graphics::GetDevice();if(texture.IsValid() || !device)return texture;
            constexpr unsigned width=64,height=128;std::array<unsigned char,width*height*4> pixels{};
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const float u=(float(x)+.5f)/32-1,v=(float(y)+.5f)/height;
                const float bend=.12f*std::sin(v*9+float(variant))+.055f*std::sin(v*25+float(variant)*2);
                const float halfWidth=.15f+.035f*std::sin(v*17+float(variant));
                float alpha=std::clamp((halfWidth-std::abs(u-bend))*50,0.f,1.f);
                alpha*=std::clamp((1-v)*25,0.f,1.f);
                const unsigned i=(y*width+x)*4;
                pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=static_cast<unsigned char>(alpha*255);
            }
            wi::graphics::TextureDesc desc;desc.width=width;desc.height=height;
            desc.format=wi::graphics::Format::R8G8B8A8_UNORM;desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;data.data_ptr=pixels.data();data.row_pitch=width*4;data.slice_pitch=pixels.size();
            device->CreateTexture(&desc,&data,&texture);return texture;
        }
        struct Anchor
        {
            wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY,mesh=wi::ecs::INVALID_ENTITY;
            int a=-1,b=-1,c=-1;XMFLOAT2 bary={0,0};
            float size=0,roll=0,normalSign=1;
            float width=0,height=0,offsetY=0;bool gravityAligned=false;
        };
        static bool Capture(const wi::scene::Scene& scene,const XMFLOAT3& point,
            const XMFLOAT3& normal,wi::ecs::Entity parent,Anchor& anchor)
        {
            const auto n=XMLoadFloat3(&normal);
            XMFLOAT3 start,back;XMStoreFloat3(&start,XMLoadFloat3(&point)+n*.04f);XMStoreFloat3(&back,-n);
            wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
            scene.IntersectsAll(hits,wi::primitive::Ray(start,back,0,.09f),
                wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
            for(const auto& hit:hits) {
                if(parent!=wi::ecs::INVALID_ENTITY && hit.entity!=parent &&
                    !scene.Entity_IsDescendant(hit.entity,parent))continue;
                const auto* object=scene.objects.GetComponent(hit.entity);
                const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
                if(!mesh || hit.vertexID0<0 || hit.vertexID1<0 || hit.vertexID2<0)continue;
                anchor.receiver=hit.entity;anchor.mesh=object->meshID;
                anchor.a=hit.vertexID0;anchor.b=hit.vertexID1;anchor.c=hit.vertexID2;anchor.bary=hit.bary;
                const auto a=scene.GetPositionOnSurface(hit.entity,anchor.a,anchor.b,anchor.c,{0,0});
                const auto b=scene.GetPositionOnSurface(hit.entity,anchor.a,anchor.b,anchor.c,{1,0});
                const auto c=scene.GetPositionOnSurface(hit.entity,anchor.a,anchor.b,anchor.c,{0,1});
                const auto cross=XMVector3Cross(XMLoadFloat3(&b)-XMLoadFloat3(&a),XMLoadFloat3(&c)-XMLoadFloat3(&a));
                anchor.normalSign=XMVectorGetX(XMVector3Dot(cross,n))<0?-1.f:1.f;
                return true;
            }
            return false;
        }
        static bool SurfacePose(const wi::scene::Scene& scene,const Anchor& anchor,XMFLOAT4X4& pose)
        {
            const auto* object=scene.objects.GetComponent(anchor.receiver);
            const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
            if(!mesh || object->meshID!=anchor.mesh || std::min({anchor.a,anchor.b,anchor.c})<0 ||
                size_t(std::max({anchor.a,anchor.b,anchor.c}))>=mesh->vertex_positions.size())return false;
            const auto a=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{0,0});
            const auto b=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{1,0});
            const auto c=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{0,1});
            const auto edge=XMLoadFloat3(&b)-XMLoadFloat3(&a);
            auto normal=XMVector3Cross(edge,XMLoadFloat3(&c)-XMLoadFloat3(&a))*anchor.normalSign;
            if(XMVectorGetX(XMVector3LengthSq(normal))<.00000001f)return false;
            normal=XMVector3Normalize(normal);const auto x=XMVector3Normalize(edge),y=XMVector3Cross(normal,x);
            const auto point=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,anchor.bary);
            XMFLOAT3 xx,yy,zz;XMStoreFloat3(&xx,x);XMStoreFloat3(&yy,y);XMStoreFloat3(&zz,normal);
            pose={xx.x,xx.y,xx.z,0,yy.x,yy.y,yy.z,0,zz.x,zz.y,zz.z,0,point.x,point.y,point.z,1};
            return true;
        }
        static bool Follow(wi::scene::Scene& scene,wi::ecs::Entity decal,Anchor& anchor)
        {
            const auto* object=scene.objects.GetComponent(anchor.receiver);
            const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
            auto* transform=scene.transforms.GetComponent(decal);
            if(!mesh || object->meshID!=anchor.mesh || !transform ||
                size_t(std::max({anchor.a,anchor.b,anchor.c}))>=mesh->vertex_positions.size())return false;
            const auto a=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{0,0});
            const auto b=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{1,0});
            const auto c=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,{0,1});
            const auto edge=XMLoadFloat3(&b)-XMLoadFloat3(&a);
            auto normal=XMVector3Cross(edge,XMLoadFloat3(&c)-XMLoadFloat3(&a))*anchor.normalSign;
            if(XMVectorGetX(XMVector3LengthSq(normal))<.00000001f)return false;
            normal=XMVector3Normalize(normal);const auto tangent=XMVector3Normalize(edge);
            const auto y=XMVector3Cross(normal,tangent);
            if(anchor.gravityAligned) {
                auto up=XMVectorSet(0,1,0,0);
                up-=normal*XMVector3Dot(up,normal);
                if(XMVectorGetX(XMVector3LengthSq(up))<.0001f)return false;
                up=XMVector3Normalize(up);
                anchor.roll=std::atan2(-XMVectorGetX(XMVector3Dot(up,tangent)),XMVectorGetX(XMVector3Dot(up,y)));
            }
            const auto localUp=-tangent*std::sin(anchor.roll)+y*std::cos(anchor.roll);
            XMFLOAT3 xAxis,yAxis,zAxis;XMStoreFloat3(&xAxis,tangent);XMStoreFloat3(&yAxis,y);XMStoreFloat3(&zAxis,normal);
            const XMFLOAT4X4 basis={xAxis.x,xAxis.y,xAxis.z,0,yAxis.x,yAxis.y,yAxis.z,0,
                zAxis.x,zAxis.y,zAxis.z,0,0,0,0,1};
            const auto point=scene.GetPositionOnSurface(anchor.receiver,anchor.a,anchor.b,anchor.c,anchor.bary);
            XMFLOAT3 p;XMStoreFloat3(&p,XMLoadFloat3(&point)+normal*.001f-localUp*anchor.offsetY);
            transform->ClearTransform();transform->MatrixTransform(
                XMMatrixScaling(anchor.width>0?anchor.width:anchor.size,anchor.height>0?anchor.height:anchor.size,.035f)*XMMatrixRotationZ(anchor.roll)*
                XMLoadFloat4x4(&basis)*XMMatrixTranslation(p.x,p.y,p.z));
            transform->UpdateTransform();return true;
        }
    };
}
