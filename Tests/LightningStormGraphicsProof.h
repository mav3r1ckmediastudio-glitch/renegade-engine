#pragma once
#include "../Runtime/src/RuntimeLightningStorm.h"

// Isolated, owner-playable mutant playground preparation. Reads an existing
// governed mage/Mutant fixture and creates a NEW equipment asset + PlayerStart
// reference; neither the fireball nor channel original is modified.
static bool MagicStormPlayableProof(const fs::path& project,const fs::path& output,
                                     bool stage3=false,bool natural=false) {
    using namespace renegade::bridge;
    using namespace renegade::runtime;
    nlohmann::json source;
    {std::ifstream in(project/"channel-playable.json");if(!in||!(in>>source))return false;}
    const auto projectId=source.at("projectId").get<std::string>();
    const auto sourceId=source.at("abilityId").get<std::string>();
    EquipmentAssetDocument previous;std::string error;
    if(!LoadEquipmentAsset(project.generic_u8string(),projectId,sourceId,previous,error)){
        std::cerr<<"storm source equipment: "<<error<<std::endl;return false;
    }
    auto ability=previous.equipment;
    ability.name=natural?"Lightning Storm - Natural Playground":
                 stage3?"Lightning Storm - Stage 3 Playground":
                        "Lightning Storm - Area Reference";
    if(ability.projectiles.size()!=1)return false;
    auto& b=ability.projectiles.front();
    b.fireMode=EquipmentFireMode::Storm;b.action=EquipmentAction::Cast;
    b.hitscanRangeMetres=35.f;b.stormRadiusMetres=2.5f;
    b.stormDurationSeconds=2.6f;b.stormStrikeCount=8;b.stormStrikeDamage=21.f;
    const auto saved=SaveEquipmentAsset(project.generic_u8string(),projectId,ability);
    if(!saved.succeeded){std::cerr<<"storm save: "<<saved.error<<std::endl;return false;}
    PlayerControllerSettings player;
    player.primaryEquipmentAssetId=saved.document.equipment.assetId;
    player.firstPersonArmsAssetId=ability.presentationAssetId;
    RuntimeEquipmentLoadout loadout;
    if(!loadout.Load(project.generic_u8string(),projectId,player)||!loadout.StormBinding()){
        std::cerr<<"storm load: "<<loadout.error<<std::endl;return false;
    }
    GameplayInputFrame input;input.fireDown=input.firePressed=true;
    (void)loadout.RouteStaged(input,true,false,.07f);
    if(!loadout.channelActive)return false;
    input.firePressed=false;input.fireDown=false;
    (void)loadout.RouteStaged(input,true,false,.07f);
    if(loadout.channelActive)return false;

    // Native scene and GPU proof of actual storm with distinct strikes.
    wi::scene::Scene visual;
    auto floor=visual.Entity_CreateCube("Storm reference ground");
    auto* t=visual.transforms.GetComponent(floor);
    t->Scale(XMFLOAT3{18,.08f,18});t->Translate(XMFLOAT3{0,-.13f,6});
    t->UpdateTransform();
    auto sun=visual.Entity_CreateLight("Storm sun",{0,7,4},{1,1,1},2,12,wi::scene::LightComponent::DIRECTIONAL);
    (void)sun;
    visual.weathers.Create(wi::ecs::CreateEntity());
    RuntimeLightningStorm storm;
    storm.UseStage3(visual,stage3);
    if(natural){
        storm.UseNatural(visual,true);
        storm.natural.refinement3.Warmup(visual,RuntimeLightningRefinement3::CachedPairs);
        if(!storm.natural.refinement3.Ready())return false;
    }
    ProjectileSource caster;caster.sourceAssetId=saved.document.equipment.assetId;
    caster.ownerSubjectId="Player";caster.factionId="Player";
    const XMFLOAT3 centre{0,0,6};
    std::cerr<<"S3 PROOF BEFORE BEGIN"<<std::endl;
    storm.Begin(visual,caster,centre,2.5f,2.6f,8,21.f);
    std::cerr<<"S3 PROOF AFTER BEGIN"<<std::endl;
    unsigned observed=0;
    bool valid=true,sawTravelling=false,sawBlueLight=false;
    bool sawAirborneLeader=false,sawLeaderCloseToFloor=false,sawGroundedFan=false;
    float previousRadius=-1.f;
    for(unsigned frame=0;frame<180;++frame) {
        if(stage3 && frame<35)std::cerr<<"S3 PROOF frame "<<frame<<std::endl;
        if(stage3 && frame<35)std::cerr<<"S3 BEFORE TICK"<<std::endl;
        storm.Tick(visual,1.f/60.f,[&](XMFLOAT3& point,unsigned strikeIndex){
            const float dist=std::hypot(point.x-centre.x,point.z-centre.z);
            valid=valid&&strikeIndex==observed&&
                RuntimeLightningStorm::Within(point.x,point.z,centre,2.5f) &&
                dist>=previousRadius-.005f;
            if(strikeIndex==0 && dist>.001f)valid=false;
            if(strikeIndex==7 && dist<2.15f)valid=false;
            previousRadius=dist;
            ++observed;
        });
        for(const auto& slot:storm.visible)if(slot.travelling && !stage3){
            if(slot.travelIndex>0){
                sawTravelling=true;
                if(const auto* arc=visual.transforms.GetComponent(slot.travellingArcs[0])){
                    if(arc->GetPosition().y<centre.y+.55f)sawGroundedFan=true;
                    else valid=false; // No suspended overhead fan/tent.
                }
            }
            if(const auto* head=visual.transforms.GetComponent(slot.travellingHead)){
                if(head->GetPosition().y>centre.y+1.f)sawAirborneLeader=true;
                if(head->GetPosition().y<centre.y+.6f)sawLeaderCloseToFloor=true;
            }
        }
        if(natural){
            if(auto* impact=visual.lights.GetComponent(storm.natural.refinement3.slots[0].skyLight))
                if(impact->intensity>1.f)sawBlueLight=true;
        }else if(stage3){
            if(auto* source=visual.lights.GetComponent(storm.stage3.sourceLight))
                if(source->intensity>1.f)sawBlueLight=true;
        }else if(auto* blue=visual.lights.GetComponent(storm.blueLight))
            if(blue->intensity>1.f)sawBlueLight=true;
        if(frame<8 && observed>0)valid=false; // Ground hits follow leader arrival.
        if(stage3 && frame<35)std::cerr<<"S3 AFTER TICK BEFORE UPDATE"<<std::endl;
        visual.Update(1.f/60.f);
        if(stage3 && frame<35)std::cerr<<"S3 AFTER UPDATE"<<std::endl;
        if(frame==34 && !Capture(visual,output/"lightning-storm-native.png",
                                 !natural,!natural,true,natural?.70f:1.f))return false;
        if(stage3 && (frame==14 || frame==51) &&
           !Capture(visual,output/(frame==14?"stage3-centre-bolt.png":"stage3-expanding-forks.png"),
                    true,true,true))return false;
    }
    if(!valid||observed!=8||!sawBlueLight||storm.landed!=8||storm.active)return false;
    if(!stage3&&!natural && (!sawTravelling||!sawAirborneLeader||
       !sawLeaderCloseToFloor||!sawGroundedFan))return false;
    if(storm.root==wi::ecs::INVALID_ENTITY||visual.objects.GetCount()<10 ||
       (!natural && (storm.outerRing.front()==wi::ecs::INVALID_ENTITY ||
                    storm.visible.front().travellingArcs.front()==wi::ecs::INVALID_ENTITY)))return false;
    if(stage3 && (storm.stage3.root==wi::ecs::INVALID_ENTITY ||
                  !visual.lights.Contains(storm.stage3.contactLight) ||
                  !visual.emitters.Contains(storm.stage3.impacts.front().sparks) ||
                  !visual.materials.Contains(storm.stage3.sourceCore) ||
                  !visual.materials.Contains(storm.stage3.mainCard) ||
                  !visual.materials.Contains(storm.stage3.marker)))return false;
    if(natural && (storm.natural.refinement3.root==wi::ecs::INVALID_ENTITY ||
                   !storm.natural.refinement3.Ready() ||
                   !visual.lights.Contains(storm.natural.refinement3.slots[0].impactLight)))return false;
    if(stage3){
        unsigned mainPoses=0,fanPoses=0;
        for(unsigned i=0;i<RuntimeLightningStage3::Variants;++i){
            if(!storm.stage3.bolts[i].IsValid())return false;
            mainPoses+=(storm.stage3.visitedPoseMask[0]>>i)&1u;
            fanPoses+=(storm.stage3.visitedPoseMask[1]>>i)&1u;
        }
        if(mainPoses<5u || fanPoses<3u)return false; // Catch static frame 4.
        for(size_t i=0;i<visual.names.GetCount();++i)
            if(visual.names[i].name.find("Stage3 Stage2 contact flare texture")!=std::string::npos)
                return false; // Catch the rectangular/diamond impact card.
    }
    // V13 must use ONE integrated discharge, not preserve V9's unrelated
    // static vertical strike or V10's hard geometric radial spikes.
    for(size_t i=0;i<visual.names.GetCount();++i){
        const auto& name=visual.names[i].name;
        if(name.find("Storm procedural lightning card")!=std::string::npos ||
           name.find("Storm radial ground discharge")!=std::string::npos)
            return false;
    }
    std::cerr<<"S3 PROOF BEFORE RESET"<<std::endl;
    storm.Reset(visual);
    std::cerr<<"S3 PROOF AFTER RESET"<<std::endl;
    if(visual.objects.GetCount()!=1||visual.emitters.GetCount()!=0)return false;

    // Only the private fixture's PlayerStart changes; original source
    // equipment, assembly and mutant in the isolated scene are retained.
    SceneService level;
    wi::Archive archive((project/"Content/Scenes/Main.wiscene").generic_u8string(),true);
    if(!archive.IsOpen())return false;
    level.GetScene().Serialize(archive);
    auto& scene=level.GetScene();
    auto start=ResolvePlayerStart(scene);
    if(start.resolution!=PlayerStartResolution::Success)return false;
    // Isolated visual-review fixture only: begin facing the mutant positioned
    // on +Z. The inherited V7 fixture yawed away from it, causing unattended
    // mouse-driven captures to look into empty sky. Never change user scenes.
    if(auto* cameraStart=scene.transforms.GetComponent(start.start.entity)){
        cameraStart->rotation_local={0,0,0,1};
        cameraStart->SetDirty();
        cameraStart->UpdateTransform();
    }else return false;
    size_t monsters=0;
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name.find("Burning mutant")!=std::string::npos)++monsters;
    if(monsters==0){std::cerr<<"Storm fixture missing supplied mutant"<<std::endl;return false;}
    SetPlayerControllerSettingsCommand replace(scene,start.start.entity,player);
    if(!replace.Execute())return false;
    scene.Update(0);
    wi::Archive savedScene;scene.Serialize(savedScene);
    if(!savedScene.SaveFile((project/"Content/Scenes/Main.wiscene").generic_u8string()))return false;
    std::ofstream(project/"storm-playable.json")<<nlohmann::json{
        {"projectId",projectId},{"abilityId",saved.document.equipment.assetId},
        {"assemblyId",ability.presentationAssetId},{"radius",2.5},{"duration",2.6},
        {"strikeCount",8},{"strikeDamage",21},{"mutantCount",monsters}}.dump(2);
    if(natural)std::cout<<"NATURAL LIGHTNING EXPERIMENT: 8 GOVERNED STRIKES, PREBUILT TUBULAR MESH POSES, IRREGULAR GROUND CURRENTS, IMPACT LIGHT AND SPARKS, MUTANT SCENE PASS"<<std::endl;
    else if(stage3)std::cout<<"STAGE 3 PLAYGROUND: APPROVED TEXTURED BOLT, CHARGED SPHERE, ORIGINAL DYNAMIC IMPACT LIGHT, SPARKS, TEXTURED RADIAL ELECTRICAL FORKS AND PROJECTED PREVIEW, 8 GOVERNED HITS, MUTANT SCENE PASS"<<std::endl;
    else std::cout<<"STORM V14 NATIVE INITIAL CENTRAL DESCENDING STRIKE, GROUND-ORIGIN EXPANDING ARC WAVE, NO OVERHEAD TENT OR DUPLICATE BOLT, EIGHT ARRIVALS, DAMAGE ON CONTACT, CLEANUP, MUTANT PLAYGROUND PASS"<<std::endl;
    return true;
}
