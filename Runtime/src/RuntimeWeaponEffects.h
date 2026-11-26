#pragma once
#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/LaunchSocketService.h"
#include "json.hpp"
#include "renegade/bridge/WeaponParticlePlayback.h"
#include <filesystem>
#include <fstream>
#include <map>

namespace renegade::runtime {
struct RuntimeShotProfile {
    using Entity=wi::ecs::Entity;
    struct Cue {Entity socket=0,mesh=0; float time=0,scale=1; bool casing=false;float duration=.075f;std::filesystem::path particle;};
    std::vector<Cue> fire,aim;
    std::map<std::string,std::vector<std::pair<float,std::string>>> audio;
    std::map<std::string,std::vector<Cue>> otherFlashes;
    std::string sound;
    std::string meleeAttackClip,meleeIdleClip;
    bool automatic=false,loopShotSound=false;
};
struct RuntimeWeaponEffects {
    using Entity=wi::ecs::Entity;
    using Scene=wi::scene::Scene;
    struct Burst {RuntimeShotProfile::Cue cue;Entity object=0;float age=0;XMFLOAT3 origin{},direction{};};
    std::vector<Burst> bursts;
    bridge::WeaponParticlePlayback particles;
    std::map<std::string,Entity> meshes;
    std::map<std::string,wi::audio::Sound> sounds;
    std::array<wi::audio::SoundInstance,12> voices{};
    unsigned voice=0;
    static bool Trigger(bool pressed,bool held,const RuntimeShotProfile& profile,int ammo) noexcept {
        return pressed || (held && profile.automatic && ammo>0);
    }
    Entity Mesh(Scene& scene,const std::filesystem::path& path,bool casing);
    void Prepare(Scene& scene,Entity root,const std::filesystem::path& project,const std::string& setup,RuntimeShotProfile& profile){
        if(setup.empty())return;
        std::ifstream in(project/setup);
        auto doc=nlohmann::json::parse(in,nullptr,false);
        if(!doc.is_object())return;
        if(doc.value("weaponType",std::string{})=="melee"&&doc.contains("melee")){
            profile.meleeAttackClip=doc["melee"].value("attack",std::string{});profile.meleeIdleClip=doc["melee"].value("idle",std::string{});
        }
        profile.automatic=doc.value("automaticFire",false)&&profile.meleeAttackClip.empty();
        auto safe=[&](std::filesystem::path p){p=p.lexically_normal();auto rel=p.lexically_relative(project).generic_u8string();return rel.rfind("Content/",0)==0?p:std::filesystem::path{};};
        profile.loopShotSound=profile.automatic&&doc.value("loopShotSound",false);
        auto sound=safe(project/doc.value("shotSound",std::string{}));
        if(!sound.empty() && std::filesystem::is_regular_file(sound)){
            auto key=sound.u8string();
            if(!sounds.count(key)){wi::audio::Sound data;if(wi::audio::CreateSound(key,&data))sounds.emplace(key,std::move(data));}
            if(sounds.count(key))profile.sound=key;
        }
        if(auto it=doc.find("animationSounds");it!=doc.end() && it->is_object()){
            for(auto clip=it->begin();clip!=it->end();++clip){
                if(!clip->is_array())continue;
                for(const auto& entry:*clip){
                    if(!entry.is_array()||entry.size()!=2||!entry[0].is_number()||!entry[1].is_string())continue;
                    float time=entry[0].get<float>();
                    auto path=safe(project/std::filesystem::u8path(entry[1].get<std::string>()));
                    if(path.empty()||!std::isfinite(time)||time<0)continue;
                    auto key=path.u8string();
                    if(!sounds.count(key)){wi::audio::Sound data;if(!wi::audio::CreateSound(key,&data))continue;sounds.emplace(key,std::move(data));}
                    profile.audio[clip.key()].emplace_back(time,key);
                }
            }
        }
        if(!profile.meleeAttackClip.empty()){
            const auto clips=doc["melee"].find("extraClips");
            const char* roles[]={"Attack2","Attack3","BlockIn","BlockHold","BlockOut"};
            if(clips!=doc["melee"].end() && clips->is_array() && clips->size()==5){
                for(size_t i=0;i<5;++i)if((*clips)[i].is_string()){
                    auto source=profile.audio.find((*clips)[i].get<std::string>());
                    if(source!=profile.audio.end())profile.audio[roles[i]]=source->second;
                }
            }
        }
        // Refresh existing published markers with the creator's latest local edits.
        if(doc.contains("launchSockets")){
            auto encoded=doc["launchSockets"];
            if(encoded.is_object() && encoded.contains("sockets") && encoded["sockets"].is_array()){
                if(doc.contains("casingEjectionMarker"))encoded["sockets"].push_back(doc["casingEjectionMarker"]);
                std::vector<bridge::LaunchSocketDefinition> edited;std::string error;
                if(bridge::ParseLaunchSockets(encoded.dump(),edited,error))for(const auto& marker:edited){
                    auto id=bridge::FindLaunchSocketEntity(scene,root,marker.name,error);
                    if(auto transform=scene.transforms.GetComponent(id)){
                        transform->ClearTransform();
                        transform->RotateRollPitchYaw(XMFLOAT3{XMConvertToRadians(marker.rotationDegrees.x),XMConvertToRadians(marker.rotationDegrees.y),XMConvertToRadians(marker.rotationDegrees.z)});
                        transform->Translate(marker.position);transform->UpdateTransform();
                    }
                }
            }
        }
        auto cues=[&](const std::string& name,const nlohmann::json& times,Entity mesh,bool casing,float scale){
            std::string error;auto socket=bridge::FindLaunchSocketEntity(scene,root,name,error);
            if(!socket||!mesh||!times.is_object())return;
            for(auto clip:{"Fire","ADS Fire"})if(times.contains(clip)&&times[clip].is_number()){
                float time=times[clip].get<float>();if(std::isfinite(time)&&time>=0)
                    (std::string(clip)=="Fire"?profile.fire:profile.aim).push_back({socket,mesh,time,scale,casing});
            }
        };
        if(doc.contains("muzzleFlashPreview")){
            auto& f=doc["muzzleFlashPreview"];const char* styles[]={"Compact.png","Star.png","Long.png"};
            auto texture=f.value("texture",std::string{});
            auto path=texture.empty()?project/"Content/Player/WeaponFX"/styles[std::clamp(f.value("style",0),0,2)]:safe(project/std::filesystem::u8path(texture));
            auto mesh=Mesh(scene,path,false);
            if(f.contains("barrelClipTimes"))for(auto it=f["barrelClipTimes"].begin();it!=f["barrelClipTimes"].end();++it){
                cues(it.key(),it.value(),mesh,false,1);
                std::string error;auto socket=bridge::FindLaunchSocketEntity(scene,root,it.key(),error);
                if(!socket||!mesh||!it->is_object())continue;
                for(auto c=it->begin();c!=it->end();++c)if(c.key()!="Fire"&&c.key()!="ADS Fire"&&c->is_number())
                    profile.otherFlashes[c.key()].push_back({socket,mesh,c->get<float>(),1,false});
            }
        }
        if(doc.contains("barrelEffects")&&doc["barrelEffects"].is_object())for(auto& entry:doc["barrelEffects"].items()){
            auto& effect=entry.value();if(!effect.is_object())continue;
            std::string error;auto socket=bridge::FindLaunchSocketEntity(scene,root,entry.key(),error);if(!socket)continue;
            float duration=std::clamp(effect.value("flashSeconds",.075f),.01f,60.f);
            auto adjust=[&](auto& cues){for(auto& cue:cues)if(cue.socket==socket&&!cue.casing){cue.duration=duration;cue.scale=std::clamp(effect.value("flashScale",1.f),.1f,10.f);}};
            adjust(profile.fire);adjust(profile.aim);for(auto& list:profile.otherFlashes)adjust(list.second);
            if(!effect.contains("file")||!effect.contains("times")||!effect["times"].is_object())continue;
            auto asset=safe(project/std::filesystem::u8path(effect["file"].get<std::string>()));
            if(!particles.Prepare(asset))continue;
            for(auto& time:effect["times"].items()){
                RuntimeShotProfile::Cue cue;cue.socket=socket;cue.time=time.value().get<float>();cue.particle=asset;cue.duration=std::clamp(effect.value("emitSeconds",.15f),.01f,60.f);
                if(time.key()=="Fire")profile.fire.push_back(cue);else if(time.key()=="ADS Fire")profile.aim.push_back(cue);else profile.otherFlashes[time.key()].push_back(cue);
            }
        }
        if(doc.contains("casingPreview")&&doc.contains("casingEjectionMarker")){
            const auto& c=doc["casingPreview"];auto path=safe((project/setup).parent_path()/c.value("model",std::string{}));
            if(!path.empty()&&c.contains("clipTimes"))cues(doc["casingEjectionMarker"].value("name",std::string{}),c["clipTimes"],Mesh(scene,path,true),true,c.value("scale",1.f));
        }
    }
    void PlaySoundCue(const std::string& path){
        auto snd=sounds.find(path);
        if(snd!=sounds.end()){
            auto& v=voices[voice++%voices.size()];if(v.IsValid())wi::audio::Stop(&v);
            if(wi::audio::CreateSoundInstance(&snd->second,&v)){wi::audio::SetVolume(.65f,&v);wi::audio::Play(&v);}
        }
    }
    Entity lastClip=0;
    float lastClipSeconds=0;
    void AnimationFrame(Scene& scene,const RuntimeShotProfile& profile,Entity clip,bool restarted){
        auto anim=scene.animations.GetComponent(clip);
        auto name=scene.names.GetComponent(clip);
        if(!anim||!name){lastClip=0;return;}
        auto label=name->name;
        if(label.rfind("Arms / ",0)==0){
            label.erase(0,7);
            static const std::map<std::string,std::string> bindings={{"Attack","Fire"},{"AimAttack","ADS Fire"},{"Run","Sprint"},{"Equip","Raise"},{"Unequip","Lower"},{"AimIn","ADS Idle"}};
            if(label=="Attack"&&!profile.meleeAttackClip.empty())label=profile.meleeAttackClip;
            else if(label=="Idle"&&!profile.meleeIdleClip.empty())label=profile.meleeIdleClip;
            auto alias=bindings.find(label);
            if(alias!=bindings.end()&&!profile.audio.count(label)&&!profile.otherFlashes.count(label))label=alias->second;
        }
        const float now=anim->timer-anim->start;
        const float before=(lastClip!=clip||restarted)?-1.f:lastClipSeconds;
        auto crossed=[&](float t){return now<before?(t>before||t<=now):(t>before&&t<=now);};
        auto list=profile.audio.find(label);
        if(list!=profile.audio.end())for(auto& cue:list->second)if(crossed(cue.first))PlaySoundCue(cue.second);
        auto flashes=profile.otherFlashes.find(label);
        if(flashes!=profile.otherFlashes.end())for(auto cue:flashes->second)if(crossed(cue.time)){cue.time=0;bursts.push_back({cue});}
        lastClip=clip;lastClipSeconds=now;
    }
    wi::audio::SoundInstance firingVoice;
    const RuntimeShotProfile* soundingProfile=nullptr;
    void KeepFiringSound(const RuntimeShotProfile* active){
        if(soundingProfile && soundingProfile!=active){
            wi::audio::Stop(&firingVoice);firingVoice={};soundingProfile=nullptr;
        }
    }
    void Shot(const RuntimeShotProfile& profile,bool aimed){
        if(!profile.audio.count(aimed?"ADS Fire":"Fire")&&!profile.audio.count(aimed?"AimAttack":"Attack")){
            if(!profile.loopShotSound)PlaySoundCue(profile.sound);
            else if(soundingProfile!=&profile){
                KeepFiringSound(nullptr);
                auto data=sounds.find(profile.sound);
                firingVoice.SetLooped(true);
                if(data!=sounds.end()&&wi::audio::CreateSoundInstance(&data->second,&firingVoice)){
                    soundingProfile=&profile;wi::audio::SetVolume(.65f,&firingVoice);wi::audio::Play(&firingVoice);
                }
            }
        }
        for(const auto& cue:(aimed?profile.aim:profile.fire))bursts.push_back({cue});
    }
    void Update(Scene& scene,float dt){
        if(dt<=0)return;
        particles.Update(scene,dt);
        for(auto it=bursts.begin();it!=bursts.end();){
            auto& b=*it;float t=b.age-b.cue.time;b.age+=dt;
            if(t<0){++it;continue;}
            auto socket=scene.transforms.GetComponent(b.cue.socket);
            if(!b.cue.particle.empty()){if(socket)particles.Spawn(scene,b.cue.particle,socket->GetWorldMatrix(),b.cue.duration);it=bursts.erase(it);continue;}
            if(!socket||t>(b.cue.casing?.68f:b.cue.duration)){if(b.object)scene.Entity_Remove(b.object,false);it=bursts.erase(it);continue;}
            const auto socketWorld=socket->GetWorldMatrix();
            if(!b.object){
                b.object=scene.Entity_CreateObject(b.cue.casing?"Ejected case":"Muzzle flash");scene.objects.GetComponent(b.object)->meshID=b.cue.mesh;
                XMStoreFloat3(&b.origin,socketWorld.r[3]);XMStoreFloat3(&b.direction,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),socketWorld)));
            }
            auto tr=scene.transforms.GetComponent(b.object);tr->ClearTransform();
            if(b.cue.casing){
                auto p=XMLoadFloat3(&b.origin)+XMLoadFloat3(&b.direction)*(1.65f*t)+XMVectorSet(0,t*(1.3f-4.9f*t),0,0);
                tr->MatrixTransform(XMMatrixScaling(b.cue.scale,b.cue.scale,b.cue.scale)*XMMatrixRotationRollPitchYaw(9*t,4*t,11*t)*XMMatrixTranslationFromVector(p));
            }else{
                auto world=socketWorld;for(int i=0;i<3;++i)world.r[i]=XMVector3Normalize(world.r[i]);
                tr->MatrixTransform(XMMatrixScaling(b.cue.scale,b.cue.scale,b.cue.scale)*world);const float fade=std::clamp(2.f-2.f*t/b.cue.duration,.1f,1.f);scene.objects.GetComponent(b.object)->color={fade,fade,fade,fade};
            }
            tr->UpdateTransform();++it;
        }
    }
};
}
