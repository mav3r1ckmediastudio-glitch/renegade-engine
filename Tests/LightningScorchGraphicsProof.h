#pragma once
#include "../Runtime/src/RuntimeProjectileVisuals.h"

// Lightning's static impact marks must not inherit the rectangular,
// directional area painter used by a continuously swept magic beam.
static bool StandaloneLightningScorchGraphicsProof(const fs::path& output) {
    using namespace renegade;
    fs::create_directories(output);
    if(fs::exists(output/"lightning-round-scorch.png"))return false;
    wi::scene::Scene scene;
    const auto floor=scene.Entity_CreateCube("Lightning scorch reference slab");
    if(auto* t=scene.transforms.GetComponent(floor)){
        t->Scale(XMFLOAT3{3.f,.08f,3.f});
        t->Translate(XMFLOAT3{0,-.13f,4.5f});
        t->UpdateTransform();
    }
    if(auto* m=scene.materials.GetComponent(floor)){
        m->baseColor={.65f,.62f,.57f,1};
        m->SetRoughness(.93f);m->SetDirty();
    }
    scene.Entity_CreateLight("Scorch visual reference light",
        {0,7,2},{1,1,1},2.0f,14,wi::scene::LightComponent::DIRECTIONAL);
    scene.weathers.Create(wi::ecs::CreateEntity());
    scene.Update(1.f/60.f);

    runtime::RuntimeProjectileVisuals visuals;
    for(unsigned i=0;i<4;++i)visuals.PrewarmLightningScorch();
    if(visuals.lightningScorchPrewarmVariant!=4)return false;
    std::array<float,3> sizes{},rotations{};
    for(unsigned i=0;i<3;++i){
        bridge::ProjectileImpact impact;
        impact.contact.position={-1.15f+1.15f*float(i),-.05f,4.5f};
        impact.contact.normal={0,1,0};
        impact.contact.surfaceType=bridge::ImpactSurfaceType::Concrete;
        impact.damage=21.f;
        visuals.PresentLightningScorch(scene,impact,floor);
        if(visuals.impactDecals.size()!=i+1 || scene.decals.GetCount()!=i+1 ||
           !visuals.channelScorchCanvas.patches.empty())return false;
        const auto& mark=visuals.impactDecals.back();
        const auto& a=mark.anchor;
        if(a.receiver!=floor||a.width!=0||a.height!=0||
           !(a.size>.65f&&a.size<.95f))return false;
        const auto* transform=scene.transforms.GetComponent(mark.entity);
        const auto* material=scene.materials.GetComponent(mark.entity);
        if(!transform||!material||
           !material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid())return false;
        // On a horizontal surface, decal axes must both be the same size
        // and the third axis aligned with the floor normal.
        const auto matrix=transform->GetWorldMatrix();
        const float x=XMVectorGetX(XMVector3Length(matrix.r[0]));
        const float y=XMVectorGetX(XMVector3Length(matrix.r[1]));
        const float z=XMVectorGetX(XMVector3Length(matrix.r[2]));
        if(std::abs(x-y)>.0005f || !(z>.015f&&z<.05f))return false;
        sizes[i]=a.size;rotations[i]=a.roll;
    }
    if(sizes[0]==sizes[1] || rotations[0]==rotations[1])return false;
    scene.Update(1.f/60.f);
    if(!Capture(scene,output/"lightning-round-scorch.png",
                false,false,true,.95f))return false;
    std::cout<<"LIGHTNING ROUND SCORCH DX12 PASS (3 near-circular marks, "
                "surface normal alignment, irregular variation, no beam tile painter)\n";
    return true;
}
