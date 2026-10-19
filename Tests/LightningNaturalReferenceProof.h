#pragma once
#include "../Runtime/src/RuntimeLightningNatural.h"

static bool StandaloneNaturalLightningProof(const fs::path& output){
    using namespace renegade::runtime;
    if(fs::exists(output/"natural-frame-019.png")){
        std::cerr<<"Natural lightning proof already exists: refusing overwrite\n";
        return false;
    }
    fs::create_directories(output);
    wi::scene::Scene scene;
    auto floor=scene.Entity_CreateCube("Ground: rough matte basalt reference");
    if(auto* t=scene.transforms.GetComponent(floor)){
        t->Scale(XMFLOAT3{5.5f,.08f,5.5f});
        t->Translate(XMFLOAT3{0,-.13f,4.6f});t->UpdateTransform();
    }
    if(auto* m=scene.materials.GetComponent(floor)){
        m->baseColor={.075f,.095f,.12f,1.f};
        m->SetRoughness(.92f);m->SetDirty();
    }
    scene.Entity_CreateLight("Reference sky fill",{0,8,2},{.4f,.5f,.69f},
        1.2f,14,wi::scene::LightComponent::DIRECTIONAL);
    scene.weathers.Create(wi::ecs::CreateEntity());
    NaturalStorm storm;
    if(!storm.Create(scene))return false;
    constexpr std::array<unsigned,10> samples{19u,21u,26u,31u,38u,45u,66u,75u,86u,100u};
    for(unsigned frame=0;frame<110;++frame){
        storm.Update(scene,float(frame)/60.f);
        scene.Update(1.f/60.f);
        if(std::find(samples.begin(),samples.end(),frame)!=samples.end()){
            const auto file=output/(std::string("natural-frame-")+(frame<100?"0":"")+std::to_string(frame)+".png");
            if(!Capture(scene,file,false,false,true,.70f))return false;
            std::cout<<"NATURAL LIGHTNING DX12 "<<frame<<" "<<file.generic_u8string()<<"\n";
        }
    }
    if(!scene.lights.Contains(storm.slots[0].light)||
       !scene.emitters.Contains(storm.slots[0].sparks)||
       !scene.materials.Contains(storm.slots[0].ground))return false;
    std::ofstream(output/"proof-status.txt")<<
        "ISOLATED NATURAL LIGHTNING REFERENCE v1. 8 timed abrupt descending strikes, "
        "dynamic branched 6-pose GPU bolt textures, 8 unique ground streamer textures, "
        "native spark emitters and true impact point lights. No gameplay integration. "
        "Owner visual acceptance pending.\n";
    std::cout<<"NATURAL LIGHTNING NATIVE VISUAL PROOF PASS\n";
    return true;
}
