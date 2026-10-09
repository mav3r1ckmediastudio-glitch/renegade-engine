#pragma once
#include "RuntimeImpactTextures.h"
#include <wiScene.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace renegade::runtime
{
    // Presentation only: no gameplay colliders or rigid bodies are created.
    struct RuntimeImpactGeometry
    {
        enum class Kind { Glass, Crown, Ripple, WaterDrop, Rock, Wood };
        static constexpr const char* RippleTag="renegade.impact.water_ripple";
        struct Piece
        {
            wi::ecs::Entity entity;
            Kind kind;
            XMFLOAT3 origin, velocity, spin, dimensions;
            XMFLOAT4X4 basis;
            float age=0, life=1, delay=0, opacity=1;
            unsigned variant=0;
        };
        static constexpr size_t MaxPieces=128;
        std::vector<Piece> pieces;

        static XMFLOAT4X4 Basis(const XMFLOAT3& direction)
        {
            auto normal=XMLoadFloat3(&direction);
            normal=XMVectorGetX(XMVector3LengthSq(normal))>.000001f?
                XMVector3Normalize(normal):XMVectorSet(0,1,0,0);
            const auto helper=std::abs(XMVectorGetY(normal))>.95f?
                XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto tangent=XMVector3Normalize(XMVector3Cross(helper,normal));
            const auto bitangent=XMVector3Cross(tangent,normal);
            XMFLOAT3 x,y,z;XMStoreFloat3(&x,tangent);XMStoreFloat3(&y,normal);XMStoreFloat3(&z,bitangent);
            return {x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1};
        }

        static const char* DebrisKey(Kind kind)
        {
            return kind==Kind::Glass?"renegade.impact.glass_shards":
                kind==Kind::Wood?"renegade.impact.wood_shards":"renegade.impact.rock_shards";
        }
        static wi::ecs::Entity FindDebrisMaterial(const wi::scene::Scene& scene,Kind kind)
        {
            const auto key=DebrisKey(kind);
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas[i].bool_values.has(key) && scene.metadatas[i].bool_values.get(key)) {
                    const auto entity=scene.metadatas.GetEntity(i);
                    const auto* material=scene.materials.GetComponent(entity);
                    if(material && material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
                       material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid())
                        return entity;
                }
            return wi::ecs::INVALID_ENTITY;
        }
        static wi::ecs::Entity Create(wi::scene::Scene& scene,Kind kind,unsigned variant=0)
        {
            const auto entity=wi::ecs::CreateEntity();
            scene.names.Create(entity).name=kind==Kind::Glass?"Impact glass shard":
                kind==Kind::Wood?"Impact wood splinter":kind==Kind::Rock?"Impact rock chip":
                kind==Kind::Crown?"Impact splash crown":kind==Kind::WaterDrop?"Impact water droplet":"Impact water ripple";
            scene.transforms.Create(entity);
            auto& object=scene.objects.Create(entity);object.meshID=entity;object.SetCastShadow(false);
            auto& material=scene.materials.Create(entity);
            material.baseColor=kind==Kind::Glass?XMFLOAT4{.85f,.92f,.96f,.65f}:XMFLOAT4{.85f,.94f,.98f,.42f};
            material.userBlendMode=wi::enums::BLENDMODE_ALPHA;
            material.SetRoughness(kind==Kind::Glass?.08f:.15f);
            material.SetReflectance(1.f);material.SetCastShadow(false);material.SetDoubleSided(true);
            // A faint glint remains readable in unlit authoring fixtures, without bloom.
            material.emissiveColor={.7f,.8f,.9f,kind==Kind::Glass?.04f:.08f};
            if(kind!=Kind::Glass) {
                material.baseColor={.98f,.995f,1.f,.85f};
                material.SetRoughness(.025f);material.SetReflectance(.08f);
                material.SetTransmissionAmount(.98f);material.SetRefractionAmount(.003f);
                material.emissiveColor={0,0,0,0};
            }
            auto& mesh=scene.meshes.Create(entity);mesh.SetDoubleSided(true);
            const auto triangle=[&](const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& c)
            {
                const auto ab=XMVectorSubtract(XMLoadFloat3(&b),XMLoadFloat3(&a));
                const auto ac=XMVectorSubtract(XMLoadFloat3(&c),XMLoadFloat3(&a));
                XMFLOAT3 n;XMStoreFloat3(&n,XMVector3Normalize(XMVector3Cross(ab,ac)));
                const auto first=unsigned(mesh.vertex_positions.size());
                for(const auto& v:{a,b,c}){mesh.vertex_positions.push_back(v);mesh.vertex_normals.push_back(n);}
                mesh.indices.push_back(first);mesh.indices.push_back(first+1);mesh.indices.push_back(first+2);
            };
            const auto donor=(kind==Kind::Glass || kind==Kind::Rock || kind==Kind::Wood)?
                FindDebrisMaterial(scene,kind):wi::ecs::INVALID_ENTITY;
            const wi::graphics::Texture* atlas=nullptr;
            if(kind==Kind::Glass)atlas=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::GlassDebris);
            else if(kind==Kind::Rock)atlas=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::RockDebris);
            else if(kind==Kind::Wood)atlas=&GetBuiltinImpactAtlas(BuiltinImpactAtlas::WoodDebris);
            if((atlas && atlas->IsValid()) || donor!=wi::ecs::INVALID_ENTITY)
            {
                if(atlas && atlas->IsValid()) {
                    material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(*atlas);
                    material.baseColor={1,1,1,1};
                    material.userBlendMode=wi::enums::BLENDMODE_ALPHA;
                    material.SetAlphaRef(1.f);material.emissiveColor={0,0,0,0};
                    material.SetRoughness(kind==Kind::Glass?.1f:.85f);
                    material.SetReflectance(kind==Kind::Glass?.08f:.025f);
                } else material=*scene.materials.GetComponent(donor);
                material.SetCastShadow(false);material.SetDoubleSided(true);
                // Four independent static atlas shapes; they never animate into one another.
                const float u=float(variant%2)*.5f,v=float((variant/2)%2)*.5f;
                mesh.vertex_positions={{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0}};
                mesh.vertex_normals={{0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1}};
                mesh.vertex_uvset_0={{u,v+.5f},{u+.5f,v+.5f},{u,v},{u+.5f,v}};
                mesh.indices={0,2,1,1,2,3};
            }
            else if(kind==Kind::Glass)
            {
                const XMFLOAT3 a{-.7f,-.45f,-.06f},b{.85f,-.3f,-.06f},c{-.15f,1.f,-.06f};
                const XMFLOAT3 d{-.7f,-.45f,.06f},e{.85f,-.3f,.06f},f{-.15f,1.f,.06f};
                triangle(a,c,b);triangle(d,e,f);
                triangle(a,b,e);triangle(a,e,d);triangle(b,c,f);triangle(b,f,e);triangle(c,a,d);triangle(c,d,f);
            }
            else if(kind==Kind::WaterDrop)
            {
                constexpr unsigned rows=8,columns=12;
                for(unsigned row=0;row<=rows;++row)
                for(unsigned column=0;column<=columns;++column) {
                    const float latitude=float(row)*XM_PI/rows;
                    const float longitude=float(column)*XM_2PI/columns;
                    XMFLOAT3 n{std::sin(latitude)*std::cos(longitude),std::cos(latitude),
                        std::sin(latitude)*std::sin(longitude)};
                    mesh.vertex_positions.push_back(n);mesh.vertex_normals.push_back(n);
                }
                for(unsigned row=0;row<rows;++row)
                for(unsigned column=0;column<columns;++column) {
                    const unsigned a=row*(columns+1)+column,b=a+columns+1;
                    for(const auto index:{a,b,a+1,a+1,b,b+1})mesh.indices.push_back(index);
                }
            }
            else if(kind==Kind::Crown)
            {
                material.SetUseVertexColors(true);
                // Smooth curved films, irregular lips and breaks instead of a toothed hoop.
                constexpr unsigned segments=96,rows=8;
                const auto vertex=[](float angle,float t) {
                    const float height=.48f+.13f*std::sin(angle*3+.6f)+
                        .09f*std::sin(angle*5+1.2f);
                    const float radius=.52f+.55f*t+.08f*std::sin(angle*3+t*2)*t;
                    return XMFLOAT3{std::cos(angle)*radius,
                        height*(t*1.35f-t*t*.35f),std::sin(angle)*radius};
                };
                for(unsigned row=0;row<=rows;++row)
                for(unsigned i=0;i<=segments;++i) {
                    const float angle=float(i)*XM_2PI/segments,t=float(row)/rows;
                    const auto v=vertex(angle,t);
                    const auto left=vertex(angle-.001f,t),right=vertex(angle+.001f,t);
                    const auto below=vertex(angle,t-.001f),above=vertex(angle,t+.001f);
                    XMFLOAT3 n;XMStoreFloat3(&n,XMVector3Normalize(XMVector3Cross(
                        XMLoadFloat3(&above)-XMLoadFloat3(&below),XMLoadFloat3(&right)-XMLoadFloat3(&left))));
                    mesh.vertex_positions.push_back(v);mesh.vertex_normals.push_back(n);
                    // Thin films have little body opacity; the rolled lip remains readable.
                    const auto alpha=uint32_t(255.f*(.07f+.78f*std::pow(t,6.f)));
                    mesh.vertex_colors.push_back(0x00FFFFFFu|(alpha<<24u));
                }
                for(unsigned row=0;row<rows;++row)
                for(unsigned i=0;i<segments;++i) {
                    const float angle=(float(i)+.5f)*XM_2PI/segments;
                    if(row>=7 && std::sin(angle*11+.5f)>.90f)continue;
                    const unsigned a=row*(segments+1)+i,b=a+segments+1;
                    for(const auto index:{a,b,a+1,a+1,b,b+1})mesh.indices.push_back(index);
                }
            }
            else
            {
                constexpr unsigned segments=48;
                for(unsigned i=0;i<segments;++i)
                {
                    const float a=float(i)*XM_2PI/segments,b=float(i+1)*XM_2PI/segments;
                    const auto vertex=[&](float angle,bool outer)
                    {
                        const float radius=kind==Kind::Ripple?(outer?1.f:.92f):(outer?1.f:.72f);
                        const float height=kind==Kind::Crown && outer?
                            .25f+.75f*std::pow(.5f+.5f*std::sin(angle*9+.6f),3.f):0;
                        return XMFLOAT3{std::cos(angle)*radius,height,std::sin(angle)*radius};
                    };
                    const auto p=vertex(a,false),q=vertex(b,false),s=vertex(a,true),t=vertex(b,true);
                    triangle(p,t,q);triangle(p,s,t);
                }
            }
            wi::scene::MeshComponent::MeshSubset subset;subset.materialID=entity;
            subset.indexOffset=0;subset.indexCount=unsigned(mesh.indices.size());mesh.subsets.push_back(subset);
            if(wi::graphics::GetDevice())mesh.CreateRenderData();
            return entity;
        }

        void Reset(wi::scene::Scene& scene)
        {
            for(const auto& piece:pieces)scene.Entity_Remove(piece.entity);
            pieces.clear();
            auto& ripples=scene.waterRipples;
            ripples.erase(std::remove_if(ripples.begin(),ripples.end(),
                [](const auto& r){return r.textureName==RippleTag;}),ripples.end());
        }
        void Add(wi::scene::Scene& scene,Piece piece)
        {
            if(pieces.size()>=MaxPieces)
            {scene.Entity_Remove(pieces.front().entity);pieces.erase(pieces.begin());}
            piece.entity=Create(scene,piece.kind,piece.variant);pieces.push_back(piece);
        }
        void Glass(wi::scene::Scene& scene,const XMFLOAT3& point,const XMFLOAT3& normal,
            float strength,unsigned seed)
        {
            const auto basis=Basis(normal);
            const auto matrix=XMLoadFloat4x4(&basis);
            const unsigned count=std::max(4u,unsigned(12*strength));
            for(unsigned i=0;i<count;++i)
            {
                const float angle=(float(i)+float(seed%17)*.31f)*2.399963f;
                const float variation=.5f+.5f*std::sin(float(i*13+seed%101));
                XMFLOAT3 velocity;
                XMStoreFloat3(&velocity,XMVector3TransformNormal(
                    XMVectorSet(std::cos(angle)*(1.1f+variation),.65f+variation,
                        std::sin(angle)*(1.1f+variation),0),matrix));
                Piece piece;piece.kind=Kind::Glass;piece.origin=point;piece.variant=(i+seed)%4;
                piece.origin.x+=normal.x*.02f;piece.origin.y+=normal.y*.02f;piece.origin.z+=normal.z*.02f;
                piece.velocity=velocity;piece.basis=basis;piece.opacity=.65f;
                piece.dimensions={.025f+.035f*variation,.035f+.05f*variation,.035f};
                piece.spin={4.f+variation*7,angle*2,8.f-variation*4};piece.life=.85f+variation*.45f;
                Add(scene,piece);
            }
            Update(scene,0);
        }
        bool Debris(wi::scene::Scene& scene,Kind kind,const XMFLOAT3& point,
            const XMFLOAT3& normal,float strength,unsigned seed)
        {
            const auto builtin=kind==Kind::Wood?BuiltinImpactAtlas::WoodDebris:
                kind==Kind::Rock?BuiltinImpactAtlas::RockDebris:BuiltinImpactAtlas::GlassDebris;
            if(!GetBuiltinImpactAtlas(builtin).IsValid() &&
                FindDebrisMaterial(scene,kind)==wi::ecs::INVALID_ENTITY)return false;
            const auto basis=Basis(normal);const auto matrix=XMLoadFloat4x4(&basis);
            const unsigned count=std::max(4u,unsigned((kind==Kind::Wood?18:14)*strength));
            for(unsigned i=0;i<count;++i) {
                const float angle=(float(i)+float(seed%17)*.31f)*2.399963f;
                const float variation=.5f+.5f*std::sin(float(i*13+seed%101));
                Piece piece;piece.kind=kind;piece.origin=point;piece.variant=(i+seed)%4;
                piece.origin.x+=normal.x*.025f;piece.origin.y+=normal.y*.025f;piece.origin.z+=normal.z*.025f;
                XMStoreFloat3(&piece.velocity,XMVector3TransformNormal(
                    XMVectorSet(std::cos(angle)*(.5f+variation),.7f+variation*1.1f,
                        std::sin(angle)*(.5f+variation),0),matrix));
                const float size=.014f+.02f*variation;
                piece.dimensions={size,kind==Kind::Wood?size*1.8f:size,.02f};
                piece.basis=basis;piece.spin={3+variation*8,angle,7-variation*3};
                piece.life=.6f+variation*.35f;piece.opacity=1;Add(scene,piece);
            }
            Update(scene,0);return true;
        }
        void Water(wi::scene::Scene& scene,const XMFLOAT3& point,const XMFLOAT3& normal,
            float strength)
        {
            const auto basis=Basis(normal);
            Piece crown;crown.kind=Kind::Crown;
            crown.origin={point.x+normal.x*.008f,point.y+normal.y*.008f,point.z+normal.z*.008f};
            crown.basis=basis;crown.dimensions={strength,strength,strength};
            crown.life=.38f;crown.opacity=.85f;Add(scene,crown);
            const auto matrix=XMLoadFloat4x4(&basis);
            for(unsigned i=0;i<64;++i) {
                const float angle=float(i)*2.399963f;
                const float variation=.5f+.5f*std::sin(float(i*17+3));
                Piece drop;drop.kind=Kind::WaterDrop;drop.origin=crown.origin;
                XMStoreFloat3(&drop.velocity,XMVector3TransformNormal(
                    XMVectorSet(std::cos(angle)*(.45f+variation*1.1f),
                        1.7f+variation*1.5f,std::sin(angle)*(.45f+variation*1.1f),0),matrix));
                drop.dimensions={.0035f+.0035f*variation,.0045f+.0055f*variation,.0035f+.0035f*variation};
                drop.basis=basis;drop.life=.55f+variation*.2f;drop.opacity=.9f;Add(scene,drop);
            }
            // Native ripple normal distortion is a horizontal water-shader effect.
            if(wi::graphics::GetDevice() && std::abs(normal.y)>.7f) {
                auto& ripples=scene.waterRipples;
                size_t owned=std::count_if(ripples.begin(),ripples.end(),
                    [](const auto& r){return r.textureName==RippleTag;});
                if(owned>=32) {
                    const auto oldest=std::find_if(ripples.begin(),ripples.end(),
                        [](const auto& r){return r.textureName==RippleTag;});
                    ripples.erase(oldest);
                }
                scene.PutWaterRipple(point);
                auto& ripple=ripples.back();ripple.textureName=RippleTag;
                ripple.params.siz={.16f*strength,.16f*strength};
                ripple.anim.scaleX=.045f*strength;ripple.anim.scaleY=.045f*strength;
                ripple.anim.fad=.012f;
            }
            Update(scene,0);
        }
        void Update(wi::scene::Scene& scene,float dt)
        {
            for(auto it=pieces.begin();it!=pieces.end();)
            {
                it->age+=std::max(0.f,dt);
                if(it->age>=it->life+it->delay || !scene.transforms.Contains(it->entity))
                {scene.Entity_Remove(it->entity);it=pieces.erase(it);continue;}
                const float age=std::max(0.f,it->age-it->delay),fraction=age/it->life;
                XMMATRIX matrix;
                if(it->kind==Kind::Glass || it->kind==Kind::WaterDrop || it->kind==Kind::Wood || it->kind==Kind::Rock)
                {
                    const auto& p=it->origin;const auto& v=it->velocity;const auto& d=it->dimensions;
                    matrix=XMMatrixScaling(d.x,d.y,d.z)*
                        XMMatrixRotationRollPitchYaw(it->spin.x*age,it->spin.y*age,it->spin.z*age)*
                        XMLoadFloat4x4(&it->basis)*
                        XMMatrixTranslation(p.x+v.x*age,p.y+v.y*age-(it->kind==Kind::WaterDrop?4.905f:4.f)*age*age,p.z+v.z*age);
                }
                else
                {
                    const float strength=it->dimensions.x;
                    const float radius=it->kind==Kind::Crown?(.055f+.30f*fraction)*strength:
                        (.07f+.70f*fraction)*strength;
                    const float height=it->kind==Kind::Crown?
                        (.025f+.30f*std::sin(fraction*XM_PI))*strength:1.f;
                    matrix=XMMatrixScaling(radius,height,radius)*XMLoadFloat4x4(&it->basis)*
                        XMMatrixTranslation(it->origin.x,it->origin.y,it->origin.z);
                }
                auto* transform=scene.transforms.GetComponent(it->entity);
                transform->ClearTransform();transform->MatrixTransform(matrix);transform->UpdateTransform();
                auto* material=scene.materials.GetComponent(it->entity);
                if(material)
                {
                    const float fade=it->kind==Kind::Glass?std::clamp((1-fraction)*4,0.f,1.f):
                        std::clamp((1-fraction)*2,0.f,1.f);
                    material->baseColor.w=it->age<it->delay?0:it->opacity*fade;
                    material->SetDirty();
                }
                ++it;
            }
        }
    };
}
