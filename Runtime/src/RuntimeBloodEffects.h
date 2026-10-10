#pragma once
#include "RuntimeImpactTextures.h"
#include "RuntimeBloodSheets.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>

namespace renegade::runtime
{
    struct RuntimeBloodEffects
    {
        RuntimeBloodSheets liquidSheets;
        // Engineering A/B switch only; no new creator authoring workflow.
        bool directionalPrototype=std::getenv("RENEGADE_DIRECTIONAL_BLOOD")!=nullptr;
        unsigned hitSerial=0;
        struct Drop { wi::ecs::Entity entity, source; XMFLOAT3 position,velocity; float age=0,radius=.004f; };
        static constexpr float StainLifetime=600.f, DryingSeconds=90.f, FadeSeconds=30.f;
        struct Stain { wi::ecs::Entity entity; float seconds=StainLifetime; float tint=1.f,opacity=.95f; };
        static constexpr size_t MaxDrops=96,MaxStains=128;
        wi::ecs::Entity mesh=wi::ecs::INVALID_ENTITY;
        std::vector<Drop> drops;
        std::vector<Stain> stains;
        unsigned stainSerial=0;
        static const wi::graphics::Texture& StainSurface(float dry)
        {
            static std::array<wi::graphics::Texture,16> textures;
            const unsigned index=unsigned(std::clamp(dry,0.f,1.f)*15);
            auto& texture=textures[index];auto* device=wi::graphics::GetDevice();
            if(texture.IsValid() || !device)return texture;
            const float amount=float(index)/15;
            const std::array<unsigned char,4> pixel{255,
                static_cast<unsigned char>((.25f+.60f*amount)*255),0,
                static_cast<unsigned char>((.04f-.02f*amount)*255)};
            wi::graphics::TextureDesc desc;desc.width=desc.height=1;
            desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
            desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;data.data_ptr=pixel.data();data.row_pitch=4;data.slice_pitch=4;
            device->CreateTexture(&desc,&data,&texture);return texture;
        }
        static float Variation(unsigned seed)
        {
            seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;
            seed*=0x846ca68bu;seed^=seed>>16;
            return float(seed&0xffffu)/65535.f;
        }
        // Bounded fallback masks: asymmetric joined liquid lobes and satellite drops.
        // These are procedural presentation, not authored or simulated pool artwork.
        static const wi::graphics::Texture& StainMask(unsigned variant)
        {
            static std::array<wi::graphics::Texture,16> textures;
            variant%=unsigned(textures.size());
            auto& texture=textures[variant];auto* device=wi::graphics::GetDevice();
            if(texture.IsValid() || !device)return texture;
            constexpr unsigned width=256;
            struct Lobe { float x,y,rx,ry; };
            std::array<Lobe,18> lobes{};
            unsigned count=0;
            const auto random=[variant](unsigned key){return Variation(variant*197u+key*101u+37u);};
            // A variable number of overlapping broad lobes makes connected irregular pools.
            const unsigned mainCount=3u+unsigned(random(1)*4);
            for(unsigned k=0;k<mainCount;++k)
            {
                const float angle=random(3+k*5)*XM_2PI;
                const float distance=k==0?0.f:.12f+random(4+k*5)*.25f;
                lobes[count++]={std::cos(angle)*distance,std::sin(angle)*distance,
                    .19f+random(5+k*5)*.24f,.16f+random(6+k*5)*.26f};
            }
            const unsigned satellites=3u+unsigned(random(50)*8);
            for(unsigned k=0;k<satellites;++k)
            {
                const float angle=random(51+k*3)*XM_2PI;
                const float distance=.53f+random(52+k*3)*.31f;
                const float radius=.014f+random(53+k*3)*.047f;
                lobes[count++]={std::cos(angle)*distance,std::sin(angle)*distance,radius,radius};
            }
            std::array<unsigned char,width*width*4> pixels{};
            for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x)
            {
                const float u=(float(x)+.5f)*2/width-1,v=(float(y)+.5f)*2/width-1;
                float alpha=0;
                for(unsigned k=0;k<count;++k)
                {
                    const auto& l=lobes[k];const float dx=(u-l.x)/l.rx,dy=(v-l.y)/l.ry;
                    const float edge=(1.f-std::sqrt(dx*dx+dy*dy))*std::min(l.rx,l.ry)*width*.5f;
                    alpha=std::max(alpha,std::clamp(edge,0.f,1.f));
                }
                const auto i=(y*width+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;
                pixels[i+3]=static_cast<unsigned char>(alpha*255);
            }
            wi::graphics::TextureDesc desc;desc.width=desc.height=width;
            desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
            desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;data.data_ptr=pixels.data();data.row_pitch=width*4;data.slice_pitch=pixels.size();
            device->CreateTexture(&desc,&data,&texture);return texture;
        }
        static bool Belongs(const wi::scene::Scene& scene,wi::ecs::Entity entity,wi::ecs::Entity root)
        {
            if(root==wi::ecs::INVALID_ENTITY)return false;
            for(size_t i=0;i<=scene.hierarchy.GetCount() && entity!=wi::ecs::INVALID_ENTITY;++i)
            {
                if(entity==root)return true;
                const auto* h=scene.hierarchy.GetComponent(entity);entity=h?h->parentID:wi::ecs::INVALID_ENTITY;
            }
            return false;
        }
        bool Ignore(const wi::scene::Scene& scene,wi::ecs::Entity entity,wi::ecs::Entity source,
            const std::vector<wi::ecs::Entity>& excluded) const
        {
            if(Belongs(scene,entity,source))return true;
            for(const auto& d:drops)if(entity==d.entity)return true;
            for(const auto& sheet:liquidSheets.sheets)if(entity==sheet.entity)return true;
            for(const auto root:excluded)if(Belongs(scene,entity,root))return true;
            for(size_t i=0;i<scene.characters.GetCount();++i)
                if(Belongs(scene,entity,scene.characters.GetEntity(i)))return true;
            return false;
        }
        void PrepareMesh(wi::scene::Scene& scene)
        {
            if(mesh!=wi::ecs::INVALID_ENTITY && scene.meshes.Contains(mesh))return;
            mesh=scene.Entity_CreateTransform("Blood droplet resource");
            auto& material=scene.materials.Create(mesh);
            material.baseColor={.26f,.012f,.018f,1};material.SetRoughness(.18f);
            material.SetReflectance(.04f);material.SetCastShadow(false);material.emissiveColor={0,0,0,0};
            auto& geometry=scene.meshes.Create(mesh);
            constexpr unsigned lat=12,lon=24;
            for(unsigned y=0;y<=lat;++y)for(unsigned x=0;x<=lon;++x)
            {
                const float a=XM_PI*float(y)/lat,b=XM_2PI*float(x)/lon;
                const XMFLOAT3 v{std::sin(a)*std::cos(b),std::cos(a),std::sin(a)*std::sin(b)};
                geometry.vertex_positions.push_back(v);geometry.vertex_normals.push_back(v);
            }
            for(unsigned y=0;y<lat;++y)for(unsigned x=0;x<lon;++x)
            {
                const unsigned a=y*(lon+1)+x,b=a+lon+1;
                for(const auto i:{a,b,a+1,a+1,b,b+1})geometry.indices.push_back(i);
            }
            wi::scene::MeshComponent::MeshSubset subset;subset.materialID=mesh;
            subset.indexCount=unsigned(geometry.indices.size());geometry.subsets.push_back(subset);
            if(wi::graphics::GetDevice())geometry.CreateRenderData();
        }
        void Spawn(wi::scene::Scene& scene,const XMFLOAT3& point,const XMFLOAT3& direction,
            wi::ecs::Entity source,float strength,unsigned seed)
        {
            PrepareMesh(scene);
            const unsigned count=std::max(18u,unsigned(60*strength));
            const auto axis=XMVector3Normalize(XMLoadFloat3(&direction));
            const auto reference=std::abs(direction.y)>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto tangent=XMVector3Normalize(XMVector3Cross(reference,axis));
            const auto bitangent=XMVector3Cross(axis,tangent);
            for(unsigned i=0;i<count;++i)
            {
                if(drops.size()>=MaxDrops){scene.Entity_Remove(drops.front().entity);drops.erase(drops.begin());}
                const auto entity=scene.Entity_CreateTransform("Blood droplet");
                auto& object=scene.objects.Create(entity);object.meshID=mesh;object.SetCastShadow(false);
                const float angle=float(i)*2.399963f+float(seed%31);
                const float spread=.35f+.35f*float(i%3);
                Drop drop;drop.entity=entity;drop.source=source;
                drop.radius=.003f+.007f*Variation(seed*59u+i*101u+7u);
                drop.position={point.x+direction.x*.035f,point.y+direction.y*.035f,point.z+direction.z*.035f};
                drop.velocity={direction.x*(1.1f+strength)+std::cos(angle)*spread,
                    direction.y*(1.1f+strength)+.35f+float(i%4)*.12f,
                    direction.z*(1.1f+strength)+std::sin(angle)*spread};
                if(directionalPrototype)
                {
                    const float azimuth=Variation(seed*113u+i*29u)*XM_2PI;
                    const float spread=.18f+.70f*Variation(seed*79u+i*47u);
                    const float speed=2.4f+3.2f*Variation(seed*53u+i*97u);
                    const auto ray=XMVector3Normalize(axis+tangent*(std::cos(azimuth)*spread)+
                        bitangent*(std::sin(azimuth)*spread));
                    XMStoreFloat3(&drop.velocity,ray*speed);
                }
                drops.push_back(drop);
                Pose(scene,drops.back());
            }
        }
        static void Pose(wi::scene::Scene& scene,const Drop& drop)
        {
            if(auto* t=scene.transforms.GetComponent(drop.entity))
            {
                const float visible=1.f-.75f*std::clamp((drop.age-.35f)/.85f,0.f,1.f);
                const float radius=drop.radius*visible;
                t->ClearTransform();t->Scale(XMFLOAT3{radius,radius*1.4f,radius});
                t->Translate(drop.position);t->UpdateTransform();
            }
        }
        void Mark(wi::scene::Scene& scene,const XMFLOAT3& point,const XMFLOAT3& normal,
            wi::ecs::Entity target,float radius,float rotation)
        {
            if(stains.size()>=MaxStains){scene.Entity_Remove(stains.front().entity);stains.erase(stains.begin());}
            const auto entity=scene.Entity_CreateDecal("Blood surface stain","","");
            auto* material=scene.materials.GetComponent(entity);auto* transform=scene.transforms.GetComponent(entity);
            if(!material || !transform){scene.Entity_Remove(entity);return;}
            const unsigned serial=stainSerial++;
            const float tint=.76f+.24f*Variation(serial*31u+3u);
            const float opacity=.82f+.13f*Variation(serial*31u+4u);
            const auto& texture=StainMask(serial);
            if(texture.IsValid())material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(texture);
            const auto& surface=StainSurface(0);
            if(surface.IsValid())material->textures[wi::scene::MaterialComponent::SURFACEMAP].resource.SetTexture(surface);
            material->baseColor={.28f*tint,.008f*tint,.014f*tint,opacity};material->userBlendMode=wi::enums::BLENDMODE_ALPHA;
            material->SetRoughness(.25f);material->SetReflectance(.04f);material->emissiveColor={0,0,0,0};material->SetDirty();
            auto n=XMLoadFloat3(&normal);
            n=XMVectorGetX(XMVector3LengthSq(n))>.000001f?XMVector3Normalize(n):XMVectorSet(0,1,0,0);
            const auto up=std::abs(XMVectorGetY(n))>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto facing=XMMatrixInverse(nullptr,XMMatrixLookToLH(XMVectorZero(),n,up));
            XMFLOAT3 offset;XMStoreFloat3(&offset,n*.004f);
            const float scale=.45f+Variation(serial*31u+5u)*1.05f;
            const float aspect=.65f+Variation(serial*31u+6u)*.85f;
            transform->MatrixTransform(XMMatrixScaling(radius*scale,radius*scale*aspect,.018f)*XMMatrixRotationZ(rotation)*facing*
                XMMatrixTranslation(point.x+offset.x,point.y+offset.y,point.z+offset.z));
            transform->UpdateTransform();
            if(scene.transforms.Contains(target))scene.Component_Attach(entity,target);
            stains.push_back({entity,StainLifetime,tint,opacity});
        }
        void Update(wi::scene::Scene& scene,float dt,const std::vector<wi::ecs::Entity>& excluded)
        {
            if(dt<=0)return;
            liquidSheets.Update(scene,dt);
            for(auto it=drops.begin();it!=drops.end();)
            {
                // Bound query work even after a long hitch; presentation ages still expire normally.
                it->age+=dt;
                if(it->age>=1.6f || !scene.transforms.Contains(it->entity))
                {scene.Entity_Remove(it->entity);it=drops.erase(it);continue;}
                const float step=std::min(dt,.05f);
                const XMFLOAT3 next{it->position.x+it->velocity.x*step,
                    it->position.y+it->velocity.y*step-4.905f*step*step,it->position.z+it->velocity.z*step};
                const auto delta=XMVectorSubtract(XMLoadFloat3(&next),XMLoadFloat3(&it->position));
                const float length=XMVectorGetX(XMVector3Length(delta));
                const wi::scene::Scene::RayIntersectionResult* nearest=nullptr;
                wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
                if(length>.000001f)
                {
                    XMFLOAT3 direction;XMStoreFloat3(&direction,XMVector3Normalize(delta));
                    scene.IntersectsAll(hits,wi::primitive::Ray(it->position,direction,0,length),
                        wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER|wi::enums::FILTER_TERRAIN);
                    for(const auto& hit:hits)
                        if(hit.entity!=wi::ecs::INVALID_ENTITY && std::isfinite(hit.distance) &&
                            hit.distance>=0 && hit.distance<=length && !Ignore(scene,hit.entity,it->source,excluded) &&
                            (!nearest || hit.distance<nearest->distance))nearest=&hit;
                }
                if(nearest)
                {
                    const auto* object=scene.objects.GetComponent(nearest->entity);
                    const auto* targetMesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
                    const auto* material=targetMesh && nearest->subsetIndex>=0 &&
                        size_t(nearest->subsetIndex)<targetMesh->subsets.size()?
                        scene.materials.GetComponent(targetMesh->subsets[nearest->subsetIndex].materialID):nullptr;
                    if(!material || material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)
                        Mark(scene,nearest->position,nearest->normal,nearest->entity,
                            .025f+it->radius*5,float(it->entity%31)*.2f);
                    scene.Entity_Remove(it->entity);it=drops.erase(it);continue;
                }
                it->position=next;it->velocity.y-=9.81f*step;Pose(scene,*it);++it;
            }
            for(auto it=stains.begin();it!=stains.end();)
            {
                it->seconds-=dt;
                if(it->seconds<=0 || !scene.transforms.Contains(it->entity))
                {scene.Entity_Remove(it->entity);it=stains.erase(it);continue;}
                if(auto* m=scene.materials.GetComponent(it->entity))
                {
                    // Wet blood darkens as it dries; the footprint remains after droplets expire.
                    const float dry=std::clamp((StainLifetime-it->seconds)/DryingSeconds,0.f,1.f);
                    m->baseColor={(.28f-.16f*dry)*it->tint,(.008f-.004f*dry)*it->tint,
                        (.014f-.008f*dry)*it->tint,it->opacity*std::min(1.f,it->seconds/FadeSeconds)};
                    const auto& surface=StainSurface(dry);
                    if(surface.IsValid())m->textures[wi::scene::MaterialComponent::SURFACEMAP].resource.SetTexture(surface);
                    m->SetRoughness(.25f+.60f*dry);m->SetReflectance(.04f-.02f*dry);m->SetDirty();
                }
                ++it;
            }
        }
        void Reset(wi::scene::Scene& scene)
        {
            liquidSheets.Reset(scene);
            for(const auto& d:drops)scene.Entity_Remove(d.entity);
            for(const auto& s:stains)scene.Entity_Remove(s.entity);
            drops.clear();stains.clear();stainSerial=0;hitSerial=0;
            if(mesh!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(mesh);
            mesh=wi::ecs::INVALID_ENTITY;
        }
    };
}
