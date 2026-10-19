#pragma once
// Lightning visual research PROTOTYPE. Intentionally isolated from the playable
// Storm, Fireball and approved Beam. Never call this from production gameplay.
// Stage 1 only: one jagged white-blue animated bolt, no impact, AoE or marker.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace renegade::lightning_research {
struct SingleBolt {
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
    std::array<wi::graphics::Texture,Frames> textures{};
    wi::ecs::Entity card=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity key=wi::ecs::INVALID_ENTITY;
    bool Create(wi::scene::Scene& scene) {
        auto* dev=wi::graphics::GetDevice();
        if(!dev)return false;
        wi::graphics::TextureDesc desc;desc.width=W;desc.height=H;
        desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        for(unsigned frame=0;frame<Frames;++frame){
            const auto pixels=Raster(frame);
            wi::graphics::SubresourceData data;
            data.data_ptr=pixels.data();data.row_pitch=W*4;data.slice_pitch=unsigned(pixels.size());
            if(!dev->CreateTexture(&desc,&data,&textures[frame]))return false;
        }
        // Render-ready Wicked cube, flattened into ONE transparent UV card.
        // It is NOT a geometric lightning bolt; the shape lives entirely
        // in its alpha texture and can fade/twist/fork pixel by pixel.
        card=scene.Entity_CreateCube("RESEARCH one animated lightning texture");
        if(auto* o=scene.objects.GetComponent(card)){
            o->filterMask=wi::enums::FILTER_TRANSPARENT;
            o->SetCastShadow(false);
        }
        if(auto* m=scene.materials.GetComponent(card)){
            m->baseColor={1,1,1,1};
            m->emissiveColor={.52f,.79f,1.f,3.6f};
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetDoubleSided(true);m->SetCastShadow(false);m->SetDirty();
        }
        if(auto* t=scene.transforms.GetComponent(card)){
            t->ClearTransform();
            t->Scale(XMFLOAT3{1.52f,2.8f,.001f}); // Wicked cube is half-unit dimension
            t->Translate(XMFLOAT3{0,2.8f,4.5f});
            t->UpdateTransform();
        }
        key=scene.Entity_CreateLight("Bolt blue light",{},XMFLOAT3{.21f,.51f,1.f},2.2f,5.f,
            wi::scene::LightComponent::POINT);
        if(auto* t=scene.transforms.GetComponent(key)){
            t->Translate(XMFLOAT3{0,2.6f,4.5f});t->UpdateTransform();
        }
        return Select(scene,0);
    }
    bool Select(wi::scene::Scene& scene,unsigned frame) {
        if(card==wi::ecs::INVALID_ENTITY || frame>=Frames)return false;
        auto* m=scene.materials.GetComponent(card);if(!m)return false;
        m->textures[wi::scene::MaterialComponent::BASECOLORMAP]
            .resource.SetTexture(textures[frame]);
        m->SetDirty();
        if(auto* l=scene.lights.GetComponent(key))
            l->intensity=frame>=7?.2f:frame>=3?3.2f:1.5f;
        return true;
    }
};
}
static bool StandaloneLightningBoltProof(const fs::path& output) {
    using namespace renegade::lightning_research;
    if(fs::exists(output/"phase-00.png")){
        std::cerr<<"Prototype evidence folder already populated: refusing overwrite\n";
        return false;
    }
    fs::create_directories(output);
    wi::scene::Scene scene;
    auto floor=scene.Entity_CreateCube("Neutral matte floor");
    if(auto* t=scene.transforms.GetComponent(floor)){
        t->Scale(XMFLOAT3{1.5f,.06f,1.5f});
        t->Translate(XMFLOAT3{0,-.17f,4.5f});t->UpdateTransform();
    }
    if(auto* m=scene.materials.GetComponent(floor)){
        m->baseColor={.12f,.14f,.20f,1.f};
        m->SetRoughness(.96f);m->SetDirty();
    }
    scene.weathers.Create(wi::ecs::CreateEntity());
    SingleBolt bolt;
    if(!bolt.Create(scene))return false;
    for(unsigned phase=0;phase<SingleBolt::Frames;++phase){
        if(!bolt.Select(scene,phase))return false;
        scene.Update(1.f/60.f);
        const auto file=output/("phase-0"+std::to_string(phase)+".png");
        if(!Capture(scene,file,false,false,true,.86f))return false;
        std::cout<<"LIGHTNING RESEARCH CAPTURE "<<phase<<" "<<file.generic_u8string()<<"\n";
    }
    // Evidence of actually textured transparency, not a fake rod mesh.
    auto* m=scene.materials.GetComponent(bolt.card);
    if(!m || !m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid())return false;
    std::ofstream(output/"prototype-stage-1.txt")<<
        "ISOLATED TEXTURED LIGHTNING - NINE TEMPORAL POSES\n"
        "Stage 1 only: no AOE, no targeting ring, no static second strike.\n"
        "No production lightning code modified. Owner visual approval pending.\n";
    std::cout<<"RENEGADE SINGLE TEXTURED LIGHTNING RESEARCH PASS - 9 NATIVE GPU POSES\n";
    return true;
}
