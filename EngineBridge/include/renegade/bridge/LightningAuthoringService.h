#pragma once
// G4 lightning authoring, versioned on Weather entity metadata. No renderer state is serialized.
#include "renegade/bridge/CommandService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace renegade::bridge {
enum class LightningMode : int { Off=0, Sheet=1, Fork=2, Both=3 };
struct LightningAuthoringState {
    LightningMode mode=LightningMode::Off;
    float sheetPerMinute=10.0f, forkPerMinute=9.0f;
    float sheetIntensity=0.14f;
    bool thunder=true;
    float thunderVolume=0.85f;
    bool distanceDelay=true;
    float forkBranchDensity=1.0f;
    int thunderSoundSet=0; // 0=standard four, 1=all nine, 2..10=individual
};
inline LightningAuthoringState SanitizeLightning(LightningAuthoringState s) noexcept {
    if (int(s.mode)<0 || int(s.mode)>3) s.mode=LightningMode::Off;
    auto clamp=[](float x,float fallback,float lo,float hi) {
        return std::clamp(std::isfinite(x)?x:fallback,lo,hi);
    };
    s.sheetPerMinute=clamp(s.sheetPerMinute,10,0,30);
    s.forkPerMinute=clamp(s.forkPerMinute,9,0,30);
    s.sheetIntensity=clamp(s.sheetIntensity,.14f,0,.25f); // preserve approved look
    s.thunderVolume=clamp(s.thunderVolume,.85f,0,1);
    s.forkBranchDensity=clamp(s.forkBranchDensity,1.0f,0.0f,2.0f);
    s.thunderSoundSet=std::clamp(s.thunderSoundSet,0,10);
    return s;
}
inline bool HasLightning(const wi::scene::Scene& scene,wi::ecs::Entity e) noexcept {
    const auto* m=scene.metadatas.GetComponent(e);
    return scene.weathers.Contains(e) && m && m->int_values.has("renegade.lightning.schema") &&
        m->int_values.get("renegade.lightning.schema")==1;
}
inline LightningAuthoringState CaptureLightning(const wi::scene::Scene& scene,wi::ecs::Entity e) noexcept {
    LightningAuthoringState s;
    if(!HasLightning(scene,e)) return s;
    const auto& m=*scene.metadatas.GetComponent(e);
    const auto readInt=[&](const char* key,int fallback) {
        return m.int_values.has(key)?int(m.int_values.get(key)):fallback;
    };
    const auto readFloat=[&](const char* key,float fallback) {
        return m.float_values.has(key)?m.float_values.get(key):fallback;
    };
    s.mode=static_cast<LightningMode>(readInt("renegade.lightning.mode",0));
    s.sheetPerMinute=readFloat("renegade.lightning.sheet_frequency",10);
    s.forkPerMinute=readFloat("renegade.lightning.fork_frequency",9);
    s.sheetIntensity=readFloat("renegade.lightning.sheet_intensity",.14f);
    s.thunder=readInt("renegade.lightning.thunder",1)!=0;
    s.thunderVolume=readFloat("renegade.lightning.volume",.85f);
    s.distanceDelay=readInt("renegade.lightning.delay",1)!=0;
    s.forkBranchDensity=readFloat("renegade.lightning.branches",1.0f);
    s.thunderSoundSet=readInt("renegade.lightning.sound_set",0);
    return SanitizeLightning(s);
}
inline bool StoreLightning(wi::scene::Scene& scene,wi::ecs::Entity e,LightningAuthoringState s) noexcept {
    if(!scene.weathers.Contains(e))return false;
    s=SanitizeLightning(s);
    auto* m=scene.metadatas.GetComponent(e);if(!m)m=&scene.metadatas.Create(e);
    m->int_values.set("renegade.lightning.schema",1);
    m->int_values.set("renegade.lightning.mode",int(s.mode));
    m->float_values.set("renegade.lightning.sheet_frequency",s.sheetPerMinute);
    m->float_values.set("renegade.lightning.fork_frequency",s.forkPerMinute);
    m->float_values.set("renegade.lightning.sheet_intensity",s.sheetIntensity);
    m->int_values.set("renegade.lightning.thunder",s.thunder?1:0);
    m->float_values.set("renegade.lightning.volume",s.thunderVolume);
    m->int_values.set("renegade.lightning.delay",s.distanceDelay?1:0);
    m->float_values.set("renegade.lightning.branches",s.forkBranchDensity);
    m->int_values.set("renegade.lightning.sound_set",s.thunderSoundSet);
    return true;
}
inline bool SameLightning(LightningAuthoringState a,LightningAuthoringState b) noexcept {
    return a.mode==b.mode && a.thunder==b.thunder && a.distanceDelay==b.distanceDelay &&
        a.thunderSoundSet==b.thunderSoundSet && std::abs(a.forkBranchDensity-b.forkBranchDensity)<1e-5f &&
        std::abs(a.sheetPerMinute-b.sheetPerMinute)<1e-5f &&
        std::abs(a.forkPerMinute-b.forkPerMinute)<1e-5f &&
        std::abs(a.sheetIntensity-b.sheetIntensity)<1e-5f &&
        std::abs(a.thunderVolume-b.thunderVolume)<1e-5f;
}
inline void ClearLightning(wi::scene::Scene& scene,wi::ecs::Entity e) noexcept {
    auto* m=scene.metadatas.GetComponent(e);if(!m)return;
    for(const char* k:{"renegade.lightning.schema","renegade.lightning.mode",
        "renegade.lightning.thunder","renegade.lightning.delay","renegade.lightning.sound_set"})m->int_values.erase(k);
    for(const char* k:{"renegade.lightning.sheet_frequency","renegade.lightning.fork_frequency",
        "renegade.lightning.sheet_intensity","renegade.lightning.volume","renegade.lightning.branches"})m->float_values.erase(k);
}
class SetLightningCommand final:public ICommand {
    wi::scene::Scene* scene_;wi::ecs::Entity entity_;
    LightningAuthoringState before_,after_;bool existed_;
public:
    SetLightningCommand(wi::scene::Scene& scene,wi::ecs::Entity entity,LightningAuthoringState next) noexcept
       :scene_(&scene),entity_(entity),before_(CaptureLightning(scene,entity)),
        after_(SanitizeLightning(next)),existed_(HasLightning(scene,entity)) {}
    bool Execute() override {return !SameLightning(before_,after_) && StoreLightning(*scene_,entity_,after_);}
    void Undo() override {if(existed_) StoreLightning(*scene_,entity_,before_);else ClearLightning(*scene_,entity_);}
};
}
