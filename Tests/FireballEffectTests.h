#include "../Runtime/src/RuntimeProjectileVisuals.h"
static void CheckFireballEffects() {
    using namespace renegade::runtime;
    wi::scene::Scene scene;const auto palm=scene.Entity_CreateTransform("Palm");
    scene.transforms.GetComponent(palm)->RotateRollPitchYaw(XMFLOAT3{1.2f,.5f,.3f});
    scene.transforms.GetComponent(palm)->UpdateTransform();
    RuntimeFireballEffect effect;Check(effect.Create(scene,palm),"fireball create");
    const auto particleIds=effect.particles;const auto effectRoot=effect.root;
    Check(scene.emitters.GetCount()==4 && scene.lights.GetCount()==1 && scene.Entity_IsDescendant(effectRoot,palm),"bounded palm appearance");
    effect.Hand(scene,.15f,true);const float small=scene.emitters.GetComponent(particleIds[1])->size;
    const float faintLight=scene.lights.GetComponent(effect.light)->intensity;
    const auto before=effect.charge;effect.Hand(scene,0,true);
    Check(effect.charge==before && scene.emitters.GetComponent(particleIds[1])->IsPaused(),"paused charge freezes");
    effect.Hand(scene,3,true);
    Check(effect.charge==1 && scene.emitters.GetComponent(particleIds[1])->size>small,"held fireball builds to bounded intensity");
    Check(scene.lights.GetComponent(effect.light)->intensity>faintLight*100 && faintLight<.0001f,"charge light begins faint and grows strongly with fireball");
    for(unsigned i=0;i<2;++i) {
        const auto* e=scene.emitters.GetComponent(particleIds[i]);
        Check(e->count*e->life>=20 && e->count*e->life<128,"held optical layers maintain enough bounded overlap to resist birth/death flashing");
    }
    const float heldCoreOpacity=scene.materials.GetComponent(particleIds[0])->baseColor.w;
    const float heldCoreRadiance=scene.materials.GetComponent(particleIds[0])->baseColor.x;
    const auto* core=scene.emitters.GetComponent(particleIds[0]);
    const float opticalWeight=core->count*core->life*heldCoreOpacity*heldCoreOpacity*heldCoreRadiance;
    Check(std::abs(opticalWeight-100.f*.045f*.75f*.75f)<.001f,"held additive radiance preserves optical weight without reducing fade alpha precision");
    effect.Hand(scene,.016f,false);
    const float stableSize=scene.emitters.GetComponent(particleIds[1])->size;
    effect.Hand(scene,.04f,false);
    Check(scene.emitters.GetComponent(particleIds[1])->size==stableSize,"constant charge does not impose global size flicker");
    for(unsigned i=1;i<4;++i)Check(scene.emitters.GetComponent(particleIds[i])->velocity.y>0 &&
        scene.emitters.GetComponent(particleIds[i])->velocity.x==0 && scene.emitters.GetComponent(particleIds[i])->velocity.z==0,"tilted palm retains world-up emission");
    for(const auto id:particleIds)scene.transforms.GetComponent(id)->world=scene.transforms.GetComponent(palm)->world;
    XMFLOAT3 tilted;XMStoreFloat3(&tilted,XMVector3TransformNormal(XMLoadFloat3(&scene.emitters.GetComponent(particleIds[1])->velocity),XMLoadFloat4x4(&scene.transforms.GetComponent(palm)->world)));
    Check(std::abs(tilted.x)+std::abs(tilted.z)>.01f,"world-up regression fixture actually tilts emission");
    effect.RefreshEmissionFrame(scene);
    const auto* flame=scene.emitters.GetComponent(particleIds[1]);XMFLOAT3 worldRise;
    XMStoreFloat3(&worldRise,XMVector3TransformNormal(XMLoadFloat3(&flame->velocity),XMLoadFloat4x4(&flame->worldMatrix)));
    Check(std::abs(worldRise.x)<.000001f && worldRise.y>0 && std::abs(worldRise.z)<.000001f,"native GPU emission frame removes palm rotation");
    effect.Hand(scene,.1f,true);
    for(const auto id:particleIds)XMStoreFloat4x4(&scene.transforms.GetComponent(id)->world,XMMatrixTranslation(.1f,0,0));
    effect.RefreshEmissionFrame(scene);
    for(const auto id:particleIds) {
        const auto* emitter=scene.emitters.GetComponent(id);
        Check(std::abs(emitter->velocity.x+emitter->gravity.x*.1f-1.f)<.0001f,"held cloud receives native hand translation without rotating buoyancy");
    }
    const auto stableCharge=effect.charge;effect.Hand(scene,std::numeric_limits<float>::quiet_NaN(),true);
    Check(effect.charge==stableCharge && scene.emitters.GetComponent(particleIds[0])->IsPaused(),"invalid charge time fails closed");
    RuntimeProjectileVisuals visuals;auto d=MakeProjectilePreset(ProjectilePreset::Spell);
    d.projectId=GenerateStableId();d.assetId=GenerateStableId();d.fireballPresentation=true;
    Check(visuals.Prepare("","",d.projectId,d) && visuals.Spawn(scene,d.assetId,1,false,&effect),"fireball native ownership transfer");
    Check(!effect.Ready() && visuals.fireballs.at(1).root==effectRoot && visuals.fireballs.at(1).particles==particleIds && scene.emitters.GetCount()==4,"release retains particles without duplicate emitters");
    auto& flying=visuals.fireballs.at(1);flying.Flight(scene,.016f,{18,0,0});
    Check(scene.materials.GetComponent(particleIds[0])->baseColor.x>heldCoreRadiance*7,"flight restores original additive radiance");
    Check(scene.emitters.GetComponent(particleIds[0])->velocity.x==18 &&
        scene.emitters.GetComponent(particleIds[1])->velocity.x<18 &&
        scene.emitters.GetComponent(particleIds[2])->velocity.x<scene.emitters.GetComponent(particleIds[1])->velocity.x,"flight head follows and tail lags projectile");
    ProjectileSimulation empty;visuals.Sync(scene,empty,0);
    Check(visuals.fireballs.empty() && visuals.dyingFireballs.size()==1 && scene.emitters.GetComponent(particleIds[1])->count==0,"impact stops new emission and retains existing tail");
    visuals.Sync(scene,empty,1);
    Check(scene.emitters.GetCount()==0 && scene.lights.GetCount()==0 && scene.transforms.Contains(palm),"fireball retirement clears owned entities only");
    effect.Create(scene,palm);effect.Hand(scene,.2f,true);effect.Reset(scene);
    Check(scene.emitters.GetCount()==0 && scene.lights.GetCount()==0,"charge cancel clears effect");
}
