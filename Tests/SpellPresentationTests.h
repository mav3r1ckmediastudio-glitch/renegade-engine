#include "renegade/bridge/FirstPersonAssemblyService.h"
#pragma once
static void CheckSpellPresentation() {
    using namespace renegade::runtime;
    FirstPersonAssemblySettings settings;settings.armsAssetId=GenerateStableId();settings.spellPresentation=true;
    settings.parentBonePath="[\"hand_r\"]";settings.spellHoldClip=3;
    settings.pairs={{"Idle",0,0},{"Charge",1,0},{"Release",2,0}};
    std::string text,error;FirstPersonAssemblySettings reopened;
    Check(SerializeFirstPersonAssemblySettings(settings,text,error) && ParseFirstPersonAssemblySettings(text,reopened,error) && reopened.spellPresentation && reopened.weaponAssetId.empty() && reopened.spellHoldClip==3,"arms-only spell recipe roundtrip "+error);
    auto bad=nlohmann::json::parse(text);bad["spell"]["extra"]=true;
    Check(!ParseFirstPersonAssemblySettings(bad.dump(),reopened,error),"unknown spell setting accepted");
    auto invalid=settings;invalid.weaponAssetId=GenerateStableId();
    Check(!SerializeFirstPersonAssemblySettings(invalid,text,error),"spell allowed hidden weapon");
    invalid=settings;invalid.spellHoldClip=1;
    Check(!SerializeFirstPersonAssemblySettings(invalid,text,error),"spell hold reused begin clip");
    wi::scene::Scene scene;RuntimePlayerViewAnimationState state;state.initialized=state.spellPresentation=true;state.loadedShells=0;
    for(auto action:{PlayerViewAction::Idle,PlayerViewAction::Charge,PlayerViewAction::Release}) {
        const auto entity=wi::ecs::CreateEntity();auto& clip=scene.animations.Create(entity);clip.start=0;clip.end=1;
        state.clips[PlayerViewActionIndex(action)].push_back({entity,"Spell",{}});state.ownedAssemblyClips.push_back(entity);
    }
    state.spellHoldClip=wi::ecs::CreateEntity();auto& hold=scene.animations.Create(state.spellHoldClip);hold.start=0;hold.end=.7f;state.ownedAssemblyClips.push_back(state.spellHoldClip);
    const auto tick=[&](float dt,bool charge=false,bool release=false,bool cancel=false) {
        UpdateRuntimePlayerViewAnimations(scene,state,PlayerViewAction::Idle,dt,false,false,false,false,true,charge,release,false,0,0,cancel);
    };
    tick(.2f,true);Check(state.spellPhase==1 && state.loadedShells==0 && !state.oneShotPlaying,"spell begin became ammo-backed shot");
    tick(0,true,false,true);Check(state.spellPhase==1 && state.spellTime==.2f,"paused cancellation mutated casting");
    tick(.8f,true);tick(.1f,true);Check(state.spellPhase==2 && state.activeClip==state.spellHoldClip,"spell did not enter authored hold loop");
    tick(.1f,false,true);Check(state.spellPhase==3 && state.spellReleaseAccepted && state.oneShotPlaying && state.loadedShells==0,"spell release acceptance");
    tick(.1f);Check(!state.spellReleaseAccepted,"spell acceptance repeated");
    tick(1.1f);Check(state.spellPhase==0 && !state.oneShotPlaying && state.loadedShells==0,"spell recovery retained ownership or added ammo");
    tick(.1f,true);tick(.1f,false,false,true);Check(state.spellPhase==0 && !state.spellReleaseAccepted,"cancelled charge fired");
    // Fire-and-forget Natural Storm recovers with physical LMB still down.
    // Channel and charge spells continue using their ordinary hold/release.
    auto normalHold=ResolveStormOneShotAnimation(false,true,2,true,false);
    Check(normalHold.chargeHeld&&!normalHold.releasePressed,"non-storm hold behaviour changed");
    tick(.1f,true);
    Check(state.spellPhase==1,"natural storm did not begin its cast");
    auto automatic=ResolveStormOneShotAnimation(true,true,state.spellPhase,true,false);
    Check(!automatic.chargeHeld&&automatic.releasePressed,"natural storm did not request auto recovery");
    UpdateRuntimePlayerViewAnimations(scene,state,PlayerViewAction::Idle,.1f,
        false,false,false,false,true,automatic.chargeHeld,automatic.releasePressed);
    Check(state.spellPhase==3&&state.oneShotPlaying,"natural storm failed to enter release animation");
    tick(1.1f);
    Check(state.spellPhase==0&&!state.oneShotPlaying,"natural storm remained stuck after one-shot recovery");
    auto stillHeld=ResolveStormOneShotAnimation(true,true,state.spellPhase,true,false);
    Check(!stillHeld.chargeHeld&&!stillHeld.releasePressed,"held LMB retriggered natural storm");
    UpdateRuntimePlayerViewAnimations(scene,state,PlayerViewAction::Idle,.1f,
        false,false,false,false,true,stillHeld.chargeHeld,stillHeld.releasePressed);
    Check(state.spellPhase==0,"natural storm resumed hold without mouse-up");
    auto physicallyReleased=ResolveStormOneShotAnimation(true,true,state.spellPhase,false,true);
    Check(!physicallyReleased.releasePressed,"mouse-up caused duplicate natural storm release");
    auto freshPress=ResolveStormOneShotAnimation(true,false,state.spellPhase,true,false);
    Check(freshPress.chargeHeld&&!freshPress.releasePressed,"next physical press failed to re-arm cast");
    ResetRuntimePlayerViewAnimations(scene,state);Check(!state.spellPresentation && state.spellHoldClip==wi::ecs::INVALID_ENTITY,"spell reset retained hold state");
}
