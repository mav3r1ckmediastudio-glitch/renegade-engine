#pragma once
#include "RuntimeChannelVisual.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

namespace renegade::runtime {
// Shared byte-for-byte raster methods extracted from the OWNER-APPROVED Stage2 prototype.
struct ResearchLightningTexture {
    static constexpr unsigned W=512,H=1024,Frames=9;
    static unsigned Hash(unsigned v) noexcept {
        v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;
    }
    static float Random(unsigned seed) noexcept {
        return float(Hash(seed)&65535u)/65535.f;
    }
    static void Dot(std::vector<unsigned char>& pixels,float x,float y,
                    float width,float strength,bool branch) {
        if(width<.05f)return;
        const float area=std::max(2.f,width*6.f);
        const int xa=std::max(0,int(std::floor(x-area)));
        const int xb=std::min(int(W)-1,int(std::ceil(x+area)));
        const int ya=std::max(0,int(std::floor(y-area)));
        const int yb=std::min(int(H)-1,int(std::ceil(y+area)));
        for(int py=ya;py<=yb;++py)for(int px=xa;px<=xb;++px) {
            const float r=std::hypot(float(px)-x,float(py)-y)/width;
            const float white=std::exp(-r*r*3.8f)*strength;
            const float cyan=std::exp(-r*r*.38f)*strength*(branch?.72f:.87f);
            const float blue=std::exp(-r*r*.052f)*strength*(branch?.31f:.43f);
            const float opacity=std::clamp(white*.98f+cyan*.48f+blue*.24f,0.f,1.f);
            if(opacity<.004f)continue;
            const size_t pos=(size_t(py)*W+size_t(px))*4u;
            const float current=float(pixels[pos+3])/255.f;
            if(opacity<=current)continue;
            const float core=std::clamp(white*.94f+cyan*.28f,0.f,1.f);
            pixels[pos]=static_cast<unsigned char>(std::clamp(28.f+227.f*core,0.f,255.f));
            pixels[pos+1]=static_cast<unsigned char>(std::clamp(85.f+170.f*core+cyan*15.f,0.f,255.f));
            pixels[pos+2]=255;
            pixels[pos+3]=static_cast<unsigned char>(opacity*255.f);
        }
    }
    static void Stroke(std::vector<unsigned char>& pixels,float ax,float ay,
                       float bx,float by,float width,float strength,bool branch) {
        const float length=std::hypot(bx-ax,by-ay);
        const unsigned steps=std::max(1u,unsigned(std::ceil(length*.95f)));
        for(unsigned j=0;j<=steps;++j){
            const float t=float(j)/float(steps);
            const float taper=branch?std::pow(1.f-t,.76f):(.86f+.14f*std::sin(t*3.14159265f));
            Dot(pixels,ax+(bx-ax)*t,ay+(by-ay)*t,
                width*(branch?.24f+.76f*taper:taper),strength*(branch?.55f+.45f*taper:1.f),branch);
        }
    }
    static std::vector<unsigned char> Raster(unsigned frame){
        std::vector<unsigned char> rgba(size_t(W)*H*4,0);
        // Nine temporal poses: charge grows, forks appear, two rapid jagged
        // flickers, then the afterimage collapses. No looping static rods.
        constexpr std::array<float,Frames> reach{.12f,.32f,.61f,.88f,1.f,1.f,.97f,.72f,.22f};
        constexpr std::array<float,Frames> power{.42f,.65f,.83f,1.f,1.f,.92f,.77f,.38f,.16f};
        constexpr unsigned Nodes=31;
        const float length=float(H-34)*reach[frame%Frames];
        const float brightness=power[frame%Frames];
        std::array<float,Nodes+1> x{},y{};
        x[0]=float(W)*.50f;
        y[0]=15.f;
        for(unsigned i=1;i<=Nodes;++i){
            const float f=float(i)/float(Nodes);
            const float wiggle=(Random(917u+i*57u+(frame%Frames)*401u)-.5f)*(26.f+17.f*std::sin(f*3.14159265f));
            // Zigzags are short and angular, not interpolated curves.
            x[i]=std::clamp(x[i-1]*.81f+float(W)*.5f*.19f+wiggle,
                            float(W)*.30f,float(W)*.70f);
            y[i]=15.f+length*f;
        }
        for(unsigned i=0;i<Nodes;++i){
            const float step=std::clamp(2.6f+.95f*std::sin(i*2.3f),1.65f,3.6f);
            Stroke(rgba,x[i],y[i],x[i+1],y[i+1],step,brightness,false);
            // Only a few asymmetric forking points. Each fork splits into
            // jagged short segments and dies away, never straight repeating
            // thorns or symmetric herringbone/tentacle geometry.
            const float forkPick=Random(3100u+i*131u+(frame%Frames)*19u);
            if(i>=3 && i<Nodes-3 && forkPick>.71f){
                const float side=Random(100u+i*19u+frame*7u)>.5f?1.f:-1.f;
                const float dist=40.f+Random(75u+i*27u+frame*31u)*111.f;
                const float endY=std::min(float(H-8),y[i]+23.f+Random(i*33u+frame*21u)*145.f);
                const float wiggle=Random(i*97u+frame*83u)-.5f;
                const float x1=x[i]+side*dist*.31f+wiggle*19.f;
                const float y1=y[i]+(endY-y[i])*.32f;
                const float x2=x[i]+side*dist*.67f-wiggle*15.f;
                const float y2=y[i]+(endY-y[i])*.64f;
                const float tipX=x[i]+side*dist;
                Stroke(rgba,x[i],y[i],x1,y1,1.45f,brightness*.72f,true);
                Stroke(rgba,x1,y1,x2,y2,.95f,brightness*.57f,true);
                Stroke(rgba,x2,y2,tipX,endY,.55f,brightness*.44f,true);
                if(Random(1000u+i*51u+frame*29u)>.53f){
                    const float x3=x2-side*(15.f+Random(i*37u+frame*17u)*33.f);
                    const float y3=y2+18.f+Random(i*7u+frame*53u)*46.f;
                    Stroke(rgba,x2,y2,x3,y3,.48f,brightness*.36f,true);
                }
            }
        }
        return rgba;
    }
    static std::vector<unsigned char> StarPixels(unsigned phase,bool top){
        constexpr unsigned n=256;
        std::vector<unsigned char> rgba(n*n*4,0);
        const float power=top?1.f:(phase<3?.07f:phase<7?1.f:phase==7?.46f:.1f);
        const float seed=float(phase*31u);
        const float radius=top?5.5f:9.f;
        for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){
            const float dx=float(x)-127.5f,dy=float(y)-127.5f;
            const float r2=dx*dx+dy*dy;
            const float inner=std::exp(-r2/(radius*radius));
            const float halo=std::exp(-r2/(top?1350.f:1900.f));
            // Star-pointed flash, with wispy independent spikes and no
            // flat opaque circular geometry over the ground.
            const float cardinal=std::exp(-std::abs(dx)*.52f)*std::exp(-std::abs(dy)*.019f)
                                +std::exp(-std::abs(dy)*.52f)*std::exp(-std::abs(dx)*.022f);
            const float diag=std::exp(-std::abs(dx-dy)*.26f)*std::exp(-std::abs(dx+dy)*.018f)
                             +std::exp(-std::abs(dx+dy)*.26f)*std::exp(-std::abs(dx-dy)*.019f);
            const float star=.31f*cardinal+.20f*diag;
            const float a=std::clamp(power*(inner*.82f+halo*.14f+star*.72f),0.f,1.f);
            const auto pos=(size_t(y)*n+x)*4;
            rgba[pos]=static_cast<unsigned char>(205+50*std::clamp(inner+star,0.f,1.f));
            rgba[pos+1]=static_cast<unsigned char>(222+33*std::clamp(inner+star,0.f,1.f));
            rgba[pos+2]=255;
            rgba[pos+3]=static_cast<unsigned char>(255*a);
        }
        if(!top && phase>=3 && phase<=7){
            for(unsigned k=0;k<24;++k){
                const float angle=ResearchLightningTexture::Random(500u+phase*33u+k*89u)*XM_2PI;
                const float extent=36.f+ResearchLightningTexture::Random(99u+phase*3u+k*41u)*82.f;
                const float start=11.f+ResearchLightningTexture::Random(k*112u+phase*99u)*25.f;
                const float cx=128.f+std::cos(angle)*start,cy=128.f+std::sin(angle)*start;
                const float endx=128.f+std::cos(angle)*extent,endy=128.f+std::sin(angle)*extent;
                const float vx=endx-cx,vy=endy-cy;
                const int steps=std::max(1,int(std::ceil(std::hypot(vx,vy))));
                for(int step=0;step<=steps;++step){
                    const float t=float(step)/float(steps);
                    const float px=cx+vx*t,py=cy+vy*t;
                    const float strength=.58f*power*(1.f-t);
                    const int xmin=std::max(0,int(px)-3),xmax=std::min(255,int(px)+3);
                    const int ymin=std::max(0,int(py)-3),ymax=std::min(255,int(py)+3);
                    for(int yy=ymin;yy<=ymax;++yy)for(int xx=xmin;xx<=xmax;++xx){
                        const float dr=std::hypot(float(xx)-px,float(yy)-py);
                        const float a=strength*std::exp(-dr*dr*1.8f);
                        const size_t pixel=(size_t(yy)*256u+size_t(xx))*4u;
                        if(a<=float(rgba[pixel+3])/255.f)continue;
                        rgba[pixel]=218;rgba[pixel+1]=237;rgba[pixel+2]=255;
                        rgba[pixel+3]=static_cast<unsigned char>(255.f*std::clamp(a,0.f,1.f));
                    }
                }
            }
        }
        return rgba;
    }

};

struct RuntimeLightningStage3 {
    static constexpr unsigned Variants=ResearchLightningTexture::Frames, ForkCount=8, ImpactSlots=3;
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity mainCard=wi::ecs::INVALID_ENTITY,marker=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sourceCore=wi::ecs::INVALID_ENTITY,sourceHalo=wi::ecs::INVALID_ENTITY,
                    sourceStar=wi::ecs::INVALID_ENTITY,sourceLight=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity contactLight=wi::ecs::INVALID_ENTITY;
    std::array<wi::ecs::Entity,ForkCount> card{};
    struct Impact {
        wi::ecs::Entity sparks=wi::ecs::INVALID_ENTITY;
        float time=0;
        XMFLOAT3 position{};
    };
    std::array<Impact,ImpactSlots> impacts{};
    std::array<wi::graphics::Texture,Variants> bolts{};
    std::array<std::uint16_t,ForkCount> visitedPoseMask{}; // native animation proof
    wi::graphics::Texture originStar{},ringTexture{};
    XMFLOAT3 centre{};
    float radius=2.5f;
    unsigned strikeCount=8;
    float seconds=0;
    bool started=false;
    static void Move(wi::scene::Scene& scene,wi::ecs::Entity e,XMFLOAT3 p,float scale,float sy=1.f) {
        if(auto* t=scene.transforms.GetComponent(e)){
            t->ClearTransform();t->Scale(XMFLOAT3{scale,scale*sy,scale});
            t->Translate(p);t->UpdateTransform();
        }
    }
    static void Hide(wi::scene::Scene& scene,wi::ecs::Entity e) {
        // Zero-scale at a neutral scene origin: never poison the preview bounds.
        Move(scene,e,{0,0,0},.00001f);
    }
    static bool Upload(const std::vector<unsigned char>& pix,unsigned width,unsigned height,
                       wi::graphics::Texture& texture){
        auto* gpu=wi::graphics::GetDevice();if(!gpu)return false;
        wi::graphics::TextureDesc d;d.width=width;d.height=height;
        d.format=wi::graphics::Format::R8G8B8A8_UNORM;
        d.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData pixels;
        pixels.data_ptr=pix.data();pixels.row_pitch=width*4;pixels.slice_pitch=unsigned(pix.size());
        return gpu->CreateTexture(&d,&pixels,&texture);
    }
    static std::vector<unsigned char> RingPixels(){
        constexpr unsigned N=512;
        std::vector<unsigned char> pix(N*N*4,0);
        for(unsigned y=0;y<N;++y)for(unsigned x=0;x<N;++x){
            const float xx=(float(x)+.5f-N*.5f)/float(N),
                        yy=(float(y)+.5f-N*.5f)/float(N);
            const float radius=std::hypot(xx,yy);
            const float outer=std::exp(-std::pow((radius-.48f)/.0049f,2.f));
            const float inner=std::exp(-std::pow((radius-.37f)/.0035f,2.f))*.16f;
            const float soft=std::exp(-std::pow((radius-.48f)/.022f,2.f))*.085f;
            const float v=std::clamp(outer*.74f+inner+soft,0.f,1.f);
            const auto i=(size_t(y)*N+x)*4u;
            pix[i]=90;pix[i+1]=174;pix[i+2]=255;pix[i+3]=unsigned char(v*255);
        }
        return pix;
    }
    static wi::ecs::Entity Card(wi::scene::Scene& scene,wi::ecs::Entity parent,const char* name,
                                XMFLOAT4 colour,XMFLOAT4 emission) {
        const auto e=scene.Entity_CreateCube(name);
        scene.Component_Attach(e,parent,true);
        if(auto* mat=scene.materials.GetComponent(e)){
            mat->baseColor=colour;mat->emissiveColor=emission;
            mat->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            mat->SetDoubleSided(true);mat->SetCastShadow(false);mat->SetDirty();
        }
        if(auto* obj=scene.objects.GetComponent(e)){
            obj->filterMask=wi::enums::FILTER_TRANSPARENT;obj->SetCastShadow(false);
        }
        Hide(scene,e);return e;
    }
    static wi::ecs::Entity Glow(wi::scene::Scene& scene,wi::ecs::Entity parent,const char* name,
                                XMFLOAT4 colour,XMFLOAT4 emission) {
        const auto e=scene.Entity_CreateSphere(name);
        scene.Component_Attach(e,parent,true);
        if(auto* mat=scene.materials.GetComponent(e)){
            mat->baseColor=colour;mat->emissiveColor=emission;
            mat->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            mat->SetCastShadow(false);mat->SetDirty();
        }
        if(auto* obj=scene.objects.GetComponent(e)){
            obj->filterMask=wi::enums::FILTER_TRANSPARENT;obj->SetCastShadow(false);
        }
        Hide(scene,e);return e;
    }
    static void Texture(wi::scene::Scene& scene,wi::ecs::Entity e,const wi::graphics::Texture& tx){
        if(auto* m=scene.materials.GetComponent(e)){
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(tx);
            m->SetDirty();
        }
    }
    bool Create(wi::scene::Scene& scene){
        if(root!=wi::ecs::INVALID_ENTITY)return true;
        std::cerr<<"S3 create entered"<<std::endl;
        if(!wi::graphics::GetDevice())return false;
        // Stage 3 must upload ALL animated frames of the approved bolt.
        // The old implementation uploaded only pose 4 and never animated.
        for(unsigned i=0;i<Variants;++i){
            const auto bytes=ResearchLightningTexture::Raster(i);
            if(!Upload(bytes,ResearchLightningTexture::W,
                       ResearchLightningTexture::H,bolts[i]))return false;
        }        std::cerr<<"S3 SOURCE GPU"<<std::endl;
        if(!Upload(ResearchLightningTexture::StarPixels(4,true),256,256,originStar))return false;
        std::cerr<<"S3 RING GPU"<<std::endl;
        if(!Upload(RingPixels(),512,512,ringTexture))return false;
        std::cerr<<"S3 CREATE GPU textures PASS"<<std::endl;
        root=scene.Entity_CreateTransform("RESEARCH Stage3 isolated playable lightning FX");
        marker=Card(scene,root,"Stage3 ground-projected blue AoE decal",
            {.51f,.81f,1.f,.7f},{.29f,.58f,1.f,.6f});
        Texture(scene,marker,ringTexture);
        mainCard=Card(scene,root,"Stage3 Stage2 approved vertical lightning texture",
            {1,1,1,1},{.52f,.79f,1.f,3.6f});
        for(unsigned i=0;i<ForkCount;++i)
            card[i]=Card(scene,root,"Stage3 Stage2 texture expanding electric fork",
                {.88f,.94f,1.f,.85f},{.42f,.7f,1.f,2.6f});
        sourceHalo=Glow(scene,root,"Stage3 charged blue top sphere",
            {.35f,.49f,1.f,.10f},{.26f,.38f,.9f,.65f});
        sourceCore=Glow(scene,root,"Stage3 white hot source sphere",
            {.89f,.95f,1.f,.52f},{.76f,.86f,1.f,3.1f});
        sourceStar=Card(scene,root,"Stage3 textured star at source",
            {.86f,.94f,1.f,.86f},{.56f,.78f,1.f,2.75f});
        Texture(scene,sourceStar,originStar);
        sourceLight=scene.Entity_CreateLight("Stage3 charged source point light",{},
            XMFLOAT3{.31f,.44f,1.f},0.f,3.6f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(sourceLight,root,true);
        contactLight=scene.Entity_CreateLight("Stage3 ORIGINAL impact point light",{},
            XMFLOAT3{.66f,.82f,1.f},0.f,4.2f,wi::scene::LightComponent::POINT);
        scene.Component_Attach(contactLight,root,true);
        std::cerr<<"S3 impact emitter setup"<<std::endl;
        for(auto& hit:impacts){
            // Do not construct the old diamond/star billboard at impact.
            // GPU sparks and the retained true point light sell contact.
            hit.sparks=bridge::CreateProjectileEffectEmitter(scene,
                {bridge::ProjectileEffectKind::Sparks,{},.028f,0.f,.30f},{});
            scene.Component_Attach(hit.sparks,root,true);
            if(auto* em=scene.emitters.GetComponent(hit.sparks)){
                em->SetMaxParticleCount(230);em->life=.21f;em->random_life=.48f;
                em->velocity={0,3.1f,0};em->gravity={0,-8.6f,0};
                em->random_factor=1.15f;em->normal_factor=1.1f;
                em->SetCollidersDisabled(false);em->count=0;
            }
            if(auto* mat=scene.materials.GetComponent(hit.sparks)){
                mat->baseColor={.67f,.83f,1.f,.88f};
                mat->emissiveColor={.63f,.8f,1.f,2.3f};mat->SetDirty();
            }
            Hide(scene,hit.sparks);
        }
        return true;
    }
    void Reset(wi::scene::Scene& scene){
        std::cerr<<"S3 RESET entered"<<std::endl;
        if(root!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(root);
        // GPU textures are ref-counted; retain them while the scene may still
        // reference material resources, release on reassignment/reset.
        *this={};
    }
    void Preview(wi::scene::Scene& scene,XMFLOAT3 where,float r,bool visible) {
        if(!Create(scene))return;
        centre=where;radius=r;
        if(!visible){Hide(scene,marker);return;}
        if(auto* t=scene.transforms.GetComponent(marker)){
            // Ground-aligned transparent projection, no raised clockface/ticks.
            t->ClearTransform();
            t->Scale(XMFLOAT3{r*2.08f,.001f,r*2.08f});
            t->Translate(XMFLOAT3{where.x,where.y+.07f,where.z});
            t->UpdateTransform();
        }
    }
    void Begin(wi::scene::Scene& scene,XMFLOAT3 where,float r,unsigned count) {
        if(!Create(scene))return;
        centre=where;radius=r;strikeCount=std::min(count,ForkCount);
        seconds=0;started=true;
        visitedPoseMask.fill(0);
        Hide(scene,marker);
        const XMFLOAT3 source={where.x,where.y+5.56f,where.z};
        Move(scene,sourceHalo,source,.48f);
        Move(scene,sourceCore,source,.21f);
        if(auto* t=scene.transforms.GetComponent(sourceStar)){
            t->ClearTransform();t->Scale(XMFLOAT3{.76f,.76f,.001f});
            t->Translate(source);t->UpdateTransform();
        }
        if(auto* t=scene.transforms.GetComponent(sourceLight)){
            t->ClearTransform();t->Translate(source);t->UpdateTransform();
        }
        if(auto* light=scene.lights.GetComponent(sourceLight))light->intensity=3.f;
        if(auto* light=scene.lights.GetComponent(contactLight))light->intensity=0.f;
        if(auto* t=scene.transforms.GetComponent(mainCard)){
            t->ClearTransform();t->Scale(XMFLOAT3{1.52f,2.8f,.001f});
            t->Translate(XMFLOAT3{where.x,where.y+2.8f,where.z});t->UpdateTransform();
        }
        for(auto& f:card)Hide(scene,f);
        for(auto& hit:impacts){
            hit.time=0;
            if(auto* em=scene.emitters.GetComponent(hit.sparks))em->count=0;
        }
    }
    void Travel(wi::scene::Scene& scene,unsigned index,const XMFLOAT3& at,float progress){
        if(!started||index>=ForkCount)return;
        // Sample the actual nine unique bolt poses as each strike travels.
        // Previously all strikes used the same static pose 4.
        const float u=std::clamp(progress,0.f,1.f);
        const unsigned frame=std::min(Variants-1,unsigned(u*float(Variants-1)));
        const wi::ecs::Entity e=index==0?mainCard:card[index];
        visitedPoseMask[index]|=static_cast<std::uint16_t>(1u<<frame);
        Texture(scene,e,bolts[frame]);
        if(index==0){
            // Existing Storm tick may hide an idle slot before it is launched;
            // re-place the central source-to-ground card on EVERY live frame.
            if(auto* t=scene.transforms.GetComponent(mainCard)){
                t->ClearTransform();
                const float grow=std::max(.015f,std::clamp(progress,0.f,1.f));
                t->Scale(XMFLOAT3{1.52f,2.8f*grow,.001f});
                t->Translate(XMFLOAT3{centre.x,centre.y+5.56f-2.8f*grow,centre.z});
                t->UpdateTransform();
            }
            return;
        }
        const XMVECTOR origin=XMVectorSet(centre.x,centre.y+.085f,centre.z,1);
        const XMVECTOR end=XMVectorLerp(origin,
            XMVectorSet(at.x,at.y+.085f,at.z,1),std::clamp(progress,0.f,1.f));
        const auto delta=origin-end;
        const float len=XMVectorGetX(XMVector3Length(delta));
        if(len<.03f)return;
        const auto axis=XMVector3Normalize(delta);
        const auto normal=XMVectorSet(0,1,0,0);
        const auto right=XMVector3Normalize(XMVector3Cross(axis,normal));
        const auto rotation=XMQuaternionRotationMatrix(
            XMMATRIX(right,axis,normal,XMVectorSet(0,0,0,1)));
        XMFLOAT4 q;XMStoreFloat4(&q,rotation);
        XMFLOAT3 middle;XMStoreFloat3(&middle,(origin+end)*.5f);
        if(auto* t=scene.transforms.GetComponent(e)){
            t->ClearTransform();
            t->Scale(XMFLOAT3{.54f,len*.5f,.001f});
            t->Rotate(q);t->Translate(middle);t->UpdateTransform();
        }
    }
    void Land(wi::scene::Scene& scene,const XMFLOAT3& at,unsigned index){
        if(!started)return;
        auto& hit=impacts[index%ImpactSlots];
        hit.position=at;hit.time=.34f;
        if(auto* t=scene.transforms.GetComponent(hit.sparks)){
            t->ClearTransform();
            t->Translate(XMFLOAT3{at.x,at.y+.09f,at.z});t->UpdateTransform();
        }
        if(auto* em=scene.emitters.GetComponent(hit.sparks))em->count=240.f;
        if(auto* t=scene.transforms.GetComponent(contactLight)){
            t->ClearTransform();
            t->Translate(XMFLOAT3{at.x,at.y+.34f,at.z});t->UpdateTransform();
        }
        if(auto* light=scene.lights.GetComponent(contactLight))light->intensity=19.f;
    }
    void Tick(wi::scene::Scene& scene,float dt){
        if(!started || dt<=0)return;
        seconds+=dt;
        float peak=0.f;
        for(auto& hit:impacts){
            if(hit.time<=0.f){
                if(auto* em=scene.emitters.GetComponent(hit.sparks))em->count=0.f;
                continue;
            }
            hit.time=std::max(0.f,hit.time-dt);
            const float f=std::clamp(hit.time/.34f,0.f,1.f);
            // Fast transient blue-white illumination; no cartoon glowing
            // sphere or star/diamond-shaped flare geometry at the impact.
            const float attack=std::clamp((1.f-f)*8.f,0.f,1.f);
            peak=std::max(peak,attack*f*f);
            if(hit.time<=.08f)
                if(auto* em=scene.emitters.GetComponent(hit.sparks))em->count=0.f;
        }
        if(auto* light=scene.lights.GetComponent(contactLight))light->intensity=19.f*peak;
        if(seconds>.78f){
            Hide(scene,mainCard);Hide(scene,sourceCore);
            Hide(scene,sourceHalo);Hide(scene,sourceStar);
            if(auto* l=scene.lights.GetComponent(sourceLight))l->intensity=0.f;
        }else if(auto* l=scene.lights.GetComponent(sourceLight))
            l->intensity=2.4f+.75f*std::sin(seconds*19.f)*std::sin(seconds*19.f);
    }
    void EndArc(wi::scene::Scene& scene,unsigned index){
        if(index<ForkCount){
            if(index>0)Hide(scene,card[index]);
            else Hide(scene,mainCard);
        }
    }
};
} // namespace renegade::runtime
