#pragma once
// Close-framed actual Wicked/DX12 visual snapshots of Refinement 3.
// Standalone controlled presentation fixture; no authored gameplay project edits.
#include "../Runtime/src/RuntimeLightningRefinement3.h"
static bool StandaloneLightningR3VisualProof(const fs::path& output) {
    using namespace renegade::runtime;
    if(fs::exists(output/"r3-near-016.png")) {
        std::cerr<<"R3 proof destination already used; refusing overwrite\n";
        return false;
    }
    fs::create_directories(output);
    wi::scene::Scene scene;
    const auto ground=scene.Entity_CreateCube("R3 dark contact surface");
    if(auto* t=scene.transforms.GetComponent(ground)){
        t->Scale(XMFLOAT3{3.5f,.10f,3.5f});
        t->Translate(XMFLOAT3{0,-.15f,4.5f});t->UpdateTransform();
    }
    if(auto* m=scene.materials.GetComponent(ground)){
        m->baseColor={.075f,.083f,.10f,1.f};m->SetRoughness(.94f);m->SetDirty();
    }
    scene.Entity_CreateLight("R3 dim reference sky",{0,7,3},
       {.6f,.7f,.85f},.45f,11.f,wi::scene::LightComponent::DIRECTIONAL);
    scene.weathers.Create(wi::ecs::CreateEntity());
    RuntimeLightningRefinement3 fx;
    fx.Warmup(scene,RuntimeLightningRefinement3::CachedPairs);
    if(!fx.Ready())return false;
    fx.Begin(scene);
    const XMFLOAT3 centre{0,0,4.5f};
    const XMFLOAT3 side{1.1f,0,4.75f};
    const std::array<unsigned,9> samples{8u,14u,16u,18u,21u,25u,32u,45u,52u};
    for(unsigned frame=0;frame<58;++frame){
        const float t=float(frame)/60.f;
        fx.Update(scene,1.f/60.f);
        if(frame<16)fx.Travel(scene,0,centre,t/(16.f/60.f));
        if(frame==16)fx.Land(scene,centre,0);
        if(frame>30 && frame<46)fx.Travel(scene,1,side,(t-30.f/60.f)/(16.f/60.f));
        if(frame==46)fx.Land(scene,side,1);
        scene.Update(1.f/60.f);
        if(std::find(samples.begin(),samples.end(),frame)!=samples.end()){
            const auto file=output/(std::string("r3-near-")+
                (frame<10?"00":frame<100?"0":"")+std::to_string(frame)+".png");
            if(!Capture(scene,file,false,false,true,.92f))return false;
            std::cout<<"R3 NATIVE DX12 FRAME "<<frame<<" "<<file.generic_u8string()<<"\n";
        }
    }
    if(fx.contacts!=2 || fx.evolvingFrames<25 ||
        !scene.lights.Contains(fx.slots[0].impactLight) ||
        !scene.emitters.Contains(fx.slots[0].sparks) ||
        !scene.objects.Contains(fx.slots[0].core) ||
        !scene.meshes.Contains(fx.coreMeshes[0]) ||
        !scene.meshes.Contains(fx.groundMesh))return false;
    std::cout<<"R3 3D STRAND NATIVE PROOF PASS contacts="<<fx.contacts
             <<" travelling_frames="<<fx.evolvingFrames<<"\n";
    return true;
}
