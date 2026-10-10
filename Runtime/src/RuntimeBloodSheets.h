#pragma once
#include "RuntimeImpactTextures.h"
#include <wiScene.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace renegade::runtime
{
    // Transient native PBR cards: regular depth testing, authored animated normals,
    // no soft-particle intersection fade. The scene's governed material owns assets.
    struct RuntimeBloodSheets
    {
        static constexpr const char* MaterialKey="renegade.blood_sheet";
        static constexpr size_t MaxSheets=24;
        struct Sheet { wi::ecs::Entity entity; XMFLOAT3 start,direction,facing; float size,rotation,age=0,life,speed=.45f,gravity=.36f; };
        std::vector<Sheet> sheets;
        static wi::ecs::Entity FindMaterial(const wi::scene::Scene& scene)
        {
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas[i].bool_values.has(MaterialKey) &&
                   scene.metadatas[i].bool_values.get(MaterialKey) &&
                   scene.materials.Contains(scene.metadatas.GetEntity(i)))
                    return scene.metadatas.GetEntity(i);
            return wi::ecs::INVALID_ENTITY;
        }
        static void Pose(wi::scene::Scene& scene,const Sheet& sheet)
        {
            auto* transform=scene.transforms.GetComponent(sheet.entity);if(!transform)return;
            const auto n=XMLoadFloat3(&sheet.facing);
            const auto up=std::abs(sheet.facing.y)>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto facing=XMMatrixInverse(nullptr,XMMatrixLookToLH(XMVectorZero(),n,up));
            const float travel=.065f+sheet.age*sheet.speed;
            transform->ClearTransform();
            transform->MatrixTransform(XMMatrixScaling(sheet.size,1,sheet.size)*XMMatrixRotationX(XM_PIDIV2)*
                XMMatrixRotationZ(sheet.rotation)*facing*
                XMMatrixTranslation(sheet.start.x+sheet.direction.x*travel,
                    sheet.start.y+sheet.direction.y*travel-sheet.age*sheet.age*sheet.gravity*.5f,
                    sheet.start.z+sheet.direction.z*travel));
            transform->UpdateTransform();
        }
        bool Spawn(wi::scene::Scene& scene,const XMFLOAT3& point,const XMFLOAT3& direction,unsigned seed,bool directional=false,const XMFLOAT3* facing=nullptr,const XMFLOAT3* outward=nullptr)
        {
            const auto donor=FindMaterial(scene);
            const auto& builtin=GetBuiltinImpactAtlas(BuiltinImpactAtlas::Blood8x8);
            if(donor==wi::ecs::INVALID_ENTITY && !builtin.IsValid())return false;
            wi::scene::MaterialComponent material;
            if(donor!=wi::ecs::INVALID_ENTITY)material=*scene.materials.GetComponent(donor);
            if(builtin.IsValid()) {
                material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(builtin);
                material.baseColor={1,1,1,1};
                material.SetRoughness(.25f);material.SetReflectance(.04f);
            }
            if(wi::graphics::GetDevice() &&
               (!material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() ||
                !material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid()))
                return false; // Never display an untextured rectangle for an unresolved governed asset.
            for(unsigned layer=0;layer<2;++layer)
            {
                if(sheets.size()>=MaxSheets){scene.Entity_Remove(sheets.front().entity);sheets.erase(sheets.begin());}
                const bool native=wi::graphics::GetDevice()!=nullptr;
                const auto entity=native?scene.Entity_CreatePlane("Blood liquid sheet"):
                    scene.Entity_CreateTransform("Blood liquid sheet");
                if(!native)
                {
                    // CPU-only lifecycle tests mirror the native plane data without GPU allocation.
                    auto& geometry=scene.meshes.Create(entity);
                    geometry.vertex_positions={{-1,0,1},{-1,0,-1},{1,0,-1},{1,0,1}};
                    geometry.vertex_normals={{0,1,0},{0,1,0},{0,1,0},{0,1,0}};
                    geometry.vertex_uvset_0={{0,0},{0,1},{1,1},{1,0}};
                    geometry.indices={0,1,2,0,2,3};
                    wi::scene::MeshComponent::MeshSubset subset;subset.materialID=entity;subset.indexCount=6;
                    geometry.subsets.push_back(subset);
                    scene.materials.Create(entity);scene.objects.Create(entity).meshID=entity;
                }
                scene.meshes.GetComponent(entity)->SetDoubleSided(true);
                auto& m=*scene.materials.GetComponent(entity);m=material;
                m.texMulAdd={.125f,.125f,0,0};m.SetDoubleSided(true);m.SetCastShadow(false);
                m.userBlendMode=wi::enums::BLENDMODE_ALPHA;m.SetAlphaRef(1.f);m.SetDirty();
                scene.objects.GetComponent(entity)->SetCastShadow(false);
                Sheet sheet;sheet.entity=entity;sheet.start=point;sheet.direction=direction;
                sheet.facing=facing?*facing:direction;
                if(directional && outward) {
                    sheet.start.x+=outward->x*.10f+sheet.facing.x*.08f;
                    sheet.start.y+=outward->y*.10f+sheet.facing.y*.08f;
                    sheet.start.z+=outward->z*.10f+sheet.facing.z*.08f;
                }
                sheet.size=layer?.62f:1.04f;
                sheet.rotation=(float(seed%7)-3)*.13f+(layer?.65f:0);
                sheet.life=layer?.48f:.38f;
                if(directional)
                {
                    const float variation=float((seed*37u+layer*19u)%101u)/100.f;
                    sheet.size=(layer?.20f:.34f)*(.8f+variation*.45f);
                    sheet.life=(layer?.31f:.23f)*(.9f+variation*.2f);
                    sheet.rotation=variation*XM_2PI;
                    sheet.speed=2.2f+variation*1.6f;sheet.gravity=9.81f;
                }
                sheets.push_back(sheet);Pose(scene,sheets.back());
            }
            return true;
        }
        void Update(wi::scene::Scene& scene,float dt)
        {
            if(dt<=0)return;
            for(auto it=sheets.begin();it!=sheets.end();)
            {
                it->age+=dt;
                if(it->age>=it->life || !scene.objects.Contains(it->entity))
                {scene.Entity_Remove(it->entity);it=sheets.erase(it);continue;}
                const unsigned frame=std::min(63u,unsigned(it->age/it->life*64));
                if(auto* m=scene.materials.GetComponent(it->entity))
                {m->texMulAdd={.125f,.125f,float(frame%8)*.125f,float(frame/8)*.125f};m->SetDirty();}
                Pose(scene,*it);++it;
            }
        }
        void Reset(wi::scene::Scene& scene)
        {for(const auto& sheet:sheets)scene.Entity_Remove(sheet.entity);sheets.clear();}
    };
}
