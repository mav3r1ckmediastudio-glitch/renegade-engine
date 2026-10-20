#pragma once
#include "../Runtime/src/RuntimeBurningStatus.h"
static void CheckBurningStatus() {
    using namespace renegade::runtime;
    wi::scene::Scene scene;
    const auto receiver=scene.Entity_CreateTransform("Burn target");
    scene.characters.Create(receiver).health=100;
    RuntimeCharacterSystemState characters;
    RuntimeCharacterRecord character;character.stableEntityId=GenerateStableId();character.entity=receiver;
    character.authoring.factionId="Enemy";characters.characters.push_back(character);
    RuntimeCharacterPerceptionState perception;CharacterCognitionRecord cognition;cognition.characterId=character.stableEntityId;
    perception.characters.push_back(cognition);
    RuntimeCombatState combat;CharacterCombatRecord health;health.characterId=character.stableEntityId;health.entity=receiver;
    combat.characters.push_back(health);
    ProjectileImpact impact;impact.contact.targetSubjectId=character.stableEntityId;
    impact.source={RuntimePlayerKnowledgeId,GenerateStableId(),"Player",{},{}};
    impact.damage=30;impact.burning={5,4};
    unsigned damageEvents=0;
    const CombatEventEmitter emit=[&](auto event,std::string&){if(event.name=="ai.damage")++damageEvents;return true;};
    RuntimeBurningStatus burning;
    Check(!burning.Ignite(scene,characters,perception,combat,impact,false,emit),"rejected contact ignited");
    const auto immediate=ApplyProjectileCharacterImpact(scene,characters,perception,combat,impact,emit);
    Check(immediate.damageApplied && combat.characters[0].health==70,"fire impact did not use existing damage");
    Check(burning.Ignite(scene,characters,perception,combat,impact,immediate.damageApplied,emit) &&
        burning.entries.size()==1 && scene.emitters.GetCount()==RuntimeBurningStatus::EmitterCount,"burn ownership not bounded to one target");
    burning.Update(scene,characters,perception,combat,0,emit);
    Check(combat.characters[0].health==70 && burning.entries[0].remaining==4 &&
        scene.emitters[0].IsPaused(),"pause consumed burn health/duration");
    burning.Update(scene,characters,perception,combat,.10f,emit);
    Check(combat.characters[0].health==70,"burn tick ran too early");
    burning.Update(scene,characters,perception,combat,.15f,emit);
    Check(std::abs(combat.characters[0].health-68.75f)<.001f,"burn tick damage wrong");
    RefreshRuntimeCombat(scene,characters,perception,combat,0);
    Check(std::abs(combat.characters[0].health-68.75f)<.001f,"native integer health erased fractional burning damage");
    // Refreshing does not duplicate emitter ownership or stack health drains.
    const auto count=scene.emitters.GetCount();
    auto weak=impact;weak.burning={2,2};
    Check(burning.Ignite(scene,characters,perception,combat,weak,true,emit) && burning.entries.size()==1 &&
        scene.emitters.GetCount()==count && burning.entries[0].damagePerSecond==5,"weaker refresh stacked or weakened burn");
    burning.Update(scene,characters,perception,combat,5,emit);
    Check(std::abs(combat.characters[0].health-50.f)<.001f && burning.entries.empty() &&
        scene.emitters.GetCount()==0 && damageEvents==3,"duration cap/final damage/cleanup failed");
    scene.metadatas.Create(receiver).bool_values.set(RuntimeBurningStatus::ImmuneKey,true);
    Check(!burning.Ignite(scene,characters,perception,combat,impact,true,emit),"fire immune target ignited");
    scene.metadatas.GetComponent(receiver)->bool_values.set(RuntimeBurningStatus::ImmuneKey,false);
    Check(burning.Ignite(scene,characters,perception,combat,impact,true,emit),"second ignition failed");
    scene.transforms.GetComponent(receiver)->Translate(XMFLOAT3{3,0,0});scene.transforms.GetComponent(receiver)->UpdateTransform();
    burning.RefreshAppearance(scene);
    Check(std::abs(scene.emitters.GetComponent(burning.entries[0].emitters[0])->center.x-2.78f)<.001f,"burn emission did not follow receiver");
    combat.characters[0].health=1;
    burning.Update(scene,characters,perception,combat,.25f,emit);
    Check(combat.characters[0].dead && burning.entries.empty() && scene.emitters.GetCount()==0,"burn death retained status/effects");
    combat.characters[0].dead=false;combat.characters[0].health=100;
    Check(burning.Ignite(scene,characters,perception,combat,impact,true,emit),"reset proof ignition");
    Check(burning.Extinguish(scene,character.stableEntityId) && burning.entries.empty() && scene.emitters.GetCount()==0,"explicit extinguish cleanup");
    // Frame partitioning plus real combat refresh must preserve the full integral.
    scene.characters.GetComponent(receiver)->health=100;combat.characters[0].health=100;
    burning.Ignite(scene,characters,perception,combat,impact,true,emit);
    for(unsigned i=0;i<240;++i) {
        burning.Update(scene,characters,perception,combat,1.f/60.f,emit);
        RefreshRuntimeCombat(scene,characters,perception,combat,0);
    }
    Check(std::abs(combat.characters[0].health-80.f)<.005f && burning.entries.empty(),"frame partition or integer refresh changed burn total");
    combat.characters[0].health=20;scene.characters.GetComponent(receiver)->health=20;
    burning.Ignite(scene,characters,perception,combat,impact,true,emit);
    for(unsigned i=0;i<240;++i) {
        burning.Update(scene,characters,perception,combat,1.f/60.f,emit);
        RefreshRuntimeCombat(scene,characters,perception,combat,0);
    }
    Check(combat.characters[0].dead && combat.characters[0].health==0 && burning.entries.empty(),"floating burn residue prevented death");
    combat.characters[0].dead=false;
    combat.characters[0].health=.4f;scene.characters.GetComponent(receiver)->health=NativeCombatHealth(.4f);
    RefreshRuntimeCombat(scene,characters,perception,combat,0);
    Check(!combat.characters[0].dead && std::abs(combat.characters[0].health-.4f)<.001f,"native health prematurely killed fractional-health actor");
    scene.characters.GetComponent(receiver)->health=25;
    RefreshRuntimeCombat(scene,characters,perception,combat,0);
    Check(combat.characters[0].health==25,"external native health update ignored");
    burning.Ignite(scene,characters,perception,combat,impact,true,emit);
    burning.Reset(scene);
    Check(burning.entries.empty() && scene.emitters.GetCount()==0,"scene/reset burn ownership leaked");
    // Pose-following covers distinct native mapped torso, arm and head regions.
    const auto addBone=[&](const char* name,XMFLOAT3 position) {
        const auto entity=scene.Entity_CreateTransform(name);scene.Component_Attach(entity,receiver,true);
        scene.transforms.GetComponent(entity)->translation_local=position;
        scene.transforms.GetComponent(entity)->UpdateTransform();return entity;
    };
    const auto retained=scene.Entity_CreateTransform("Retained retarget source");scene.Component_Attach(retained,receiver,true);
    const auto sourceHead=addBone("retarget head",{9,9,9});
    scene.humanoids.Create(retained).bones[size_t(RuntimeBurningStatus::Bone::Head)]=sourceHead;
    auto& humanoid=scene.humanoids.Create(receiver);
    using Bone=RuntimeBurningStatus::Bone;
    humanoid.bones[size_t(Bone::Hips)]=addBone("mapped hips",{3,.85f,0});
    humanoid.bones[size_t(Bone::Chest)]=addBone("mapped chest",{3,1.35f,0});
    const auto head=addBone("mapped head",{3,1.8f,0});
    const auto arm=addBone("mapped left upper arm",{2.6f,1.4f,0});
    const auto elbow=addBone("mapped left lower arm",{2.4f,1.1f,0});
    humanoid.bones[size_t(Bone::Head)]=head;humanoid.bones[size_t(Bone::LeftUpperArm)]=arm;
    humanoid.bones[size_t(Bone::LeftLowerArm)]=elbow;
    const auto object=scene.Entity_CreateTransform("Live skin proxy");scene.Component_Attach(object,receiver,true);
    const auto mesh=wi::ecs::CreateEntity();scene.objects.Create(object).meshID=mesh;scene.meshes.Create(mesh);
    const auto rig=wi::ecs::CreateEntity();
    auto& live=scene.armatures.Create(rig);live.boneCollection={head,arm,elbow,
        humanoid.bones[size_t(Bone::Hips)],humanoid.bones[size_t(Bone::Chest)]};
    scene.meshes.GetComponent(scene.objects.GetComponent(object)->meshID)->armatureID=rig;
    Check(burning.Ignite(scene,characters,perception,combat,impact,true,emit),"mapped body ignition");
    burning.RefreshAppearance(scene);
    const auto& body=burning.entries.front();
    Check(body.bones[12]==head && body.bones[8]==arm && body.ends[8]==elbow,
        "head/arm native region mapping collapsed onto trunk");
    const auto before=scene.emitters.GetComponent(body.emitters[8])->center;
    scene.transforms.GetComponent(elbow)->Translate(XMFLOAT3{0,.4f,0});
    scene.transforms.GetComponent(elbow)->UpdateTransform();burning.RefreshAppearance(scene);
    Check(std::abs(scene.emitters.GetComponent(body.emitters[8])->center.y-before.y-.2f)<.001f,
        "arm flame did not follow native segment midpoint");
    Check(scene.emitters.GetComponent(body.emitters[12])->center.y>
        scene.emitters.GetComponent(body.emitters[0])->center.y+.7f,"head and trunk fire not distributed");
    burning.Reset(scene);

}
