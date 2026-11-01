#pragma once
// Owner-supplied Renegade thunder: four mono spatial variants from a nine-SFX library.
// Audio is independent from approved lightning visuals and isn't serialized into levels.
#include <WickedEngine.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <vector>

// Internal Windows backend observation; no shared callback/lifecycle changes.
namespace wi::audio { uint32_t GetQueuedBufferCount(const SoundInstance* instance); }
namespace renegade::runtime {
class RuntimeThunderAudio {
    struct Voice { wi::audio::SoundInstance instance; XMFLOAT3 origin{}; float age=0; std::uint64_t sampleCount=0; bool started=false; };
    struct Pending { XMFLOAT3 origin{}; float delay=0; unsigned variant=0; float volume=0.85f; };
    std::array<wi::audio::Sound,9> sounds_{};
    std::vector<Voice> voices_;
    std::vector<Pending> pending_;
    bool attempted_=false;
    unsigned loadAttempts_=0;
    std::uint64_t sceneRevision_=~0ull;
    unsigned sequence_=0;
    // Normalize perceived loudness for the user-authored clips without changing their WAV masters.
    static constexpr float level_[9] = {2.4f, 1.7f, 1.0f, 1.5f, 2.1f, 1.0f, 1.9f, 2.7f, 2.3f};
    static constexpr const char* files_[9] = {"thunder_lightning_01.wav","thunder_lightning_02.wav","thunder_booming_01.wav","thunder_distant_02.wav","thunder_booming_02.wav","thunder_booming_03.wav","thunder_distant_03.wav","thunder_distant_04.wav","thunder_lightning_03.wav"};
    static void Trace(const std::string& entry) {
        if(const char* path=std::getenv("RENEGADE_G4_THUNDER_TRACE")) {
            std::ofstream out(path,std::ios::app);
            if(out) out<<entry<<"\n";
        }
    }
    static wi::audio::SoundInstance3D Spatial(const XMFLOAT3& origin,
              const XMFLOAT3& listener,const XMFLOAT3& front) {
        const float dx=origin.x-listener.x,dy=origin.y-listener.y,dz=origin.z-listener.z;
        const float len=std::sqrt(dx*dx+dy*dy+dz*dz);
        // Do not give kilometre-scale coordinates to X3DAudio's normal
        // metre-scale attenuation curve: it reduces the sample to silence.
        // Preserve the true bearing; a separate physical-distance volume
        // envelope and speed-of-sound delay provide long-range behaviour.
        const float ratio=std::min(1.5f,len)/std::max(len,0.00001f);
        wi::audio::SoundInstance3D pos;
        pos.listenerPos=listener;
        // Wicked Camera::At is a direction (XMMatrixLookToLH), not a target.
        const float fx=front.x,fy=front.y,fz=front.z;
        const float flen=std::sqrt(fx*fx+fy*fy+fz*fz);
        pos.listenerFront=flen>0.001f?XMFLOAT3(fx/flen,fy/flen,fz/flen):XMFLOAT3(0,0,1);
        // X3DAudio requires orthonormal front/up, including pitched views.
        const auto f=XMLoadFloat3(&pos.listenerFront);
        const auto reference=std::abs(pos.listenerFront.y)>.99f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
        const auto right=XMVector3Normalize(XMVector3Cross(reference,f));
        XMStoreFloat3(&pos.listenerUp,XMVector3Normalize(XMVector3Cross(f,right)));
        pos.emitterPos=XMFLOAT3(listener.x+dx*ratio,listener.y+dy*ratio,listener.z+dz*ratio);
        pos.emitterRadius=1.5f;
        return pos;
    }
    static float DistantGain(const XMFLOAT3& origin,const XMFLOAT3& listener) {
        const float dx=origin.x-listener.x,dy=origin.y-listener.y,dz=origin.z-listener.z;
        const float metres=std::sqrt(dx*dx+dy*dy+dz*dz);
        return std::clamp(1.0f/(1.0f+metres/9000.0f),0.15f,1.0f);
    }
    void Load() {
        if(attempted_) return;
        // Retry if the audio device was not ready on the first strike.
        if(loadAttempts_>=24) return;
        wi::profiler::ScopedRangeCPU loadTiming("Thunder Preload");
        ++loadAttempts_;
        const auto executableAssets = std::filesystem::path(wi::helper::GetExecutablePath())
            .parent_path() / "Content/weather/thunder/default3d";
        for(unsigned i=0;i<9;++i) {
            // Load from the executable directory first: launching Renegade
            // from Explorer, Studio Test Game or a shortcut can all set a
            // different current working directory.
            wi::audio::CreateSound((executableAssets/files_[i]).string(),&sounds_[i]);
            if(sounds_[i].IsValid()) {
                Trace("LOAD_OK " + (executableAssets/files_[i]).string());
                continue;
            }
            for(const char* prefix:{"Content/weather/thunder/default3d/", "Runtime/Content/weather/thunder/default3d/", "weather/thunder/default3d/"}) {
                if(sounds_[i].IsValid()) break;
                const std::string full=std::string(prefix)+files_[i];
                if(wi::audio::CreateSound(full,&sounds_[i])) {
                    Trace("LOAD_OK "+full);break;
                }
            }
        }
        attempted_=std::all_of(sounds_.begin(),sounds_.end(),
                              [](const wi::audio::Sound& sound){return sound.IsValid();});
        if(!attempted_)Trace("LOAD_PARTIAL attempts="+std::to_string(loadAttempts_));
    }
public:
    void OnSceneRevision(std::uint64_t revision){if(sceneRevision_!=revision){Reset();sceneRevision_=revision;Load();}}
    void Queue(const XMFLOAT3& origin,const XMFLOAT3& listener,bool enabled=true,float volume=.85f,bool delay=true,int soundSet=0) {
        if(!enabled || volume<=0.0f)return;
        Load();
        // Lightning appears immediately, thunder follows sound propagation (~343 m/s).
        const float dx=origin.x-listener.x,dy=origin.y-listener.y,dz=origin.z-listener.z;
        const float metres=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(pending_.size()>=12) pending_.erase(pending_.begin());
        const unsigned choice=soundSet==0?sequence_++%4:soundSet==1?sequence_++%9:unsigned(std::clamp(soundSet-2,0,8));
        const float wait=delay?std::min(25.0f,metres/343.0f):0.0f;
        pending_.push_back({origin,wait,choice,std::clamp(volume,0.0f,1.0f)});
        Trace("QUEUE variant="+std::to_string(choice)+" delay="+std::to_string(wait)+
              " dist="+std::to_string(metres)+" loaded="+std::to_string(sounds_[choice].IsValid()));
    }
    void Update(float dt,const XMFLOAT3& listener,const XMFLOAT3& front) {
        if(dt<=0) return;
        for(auto& p:pending_) p.delay-=dt;
        auto it=pending_.begin();
        while(it!=pending_.end()) {
            if(it->delay>0) {++it;continue;}
            const auto p=*it;it=pending_.erase(it);
            if(!sounds_[p.variant].IsValid() || voices_.size()>=8) {
                Trace("PLAY_SKIP loaded="+std::to_string(sounds_[p.variant].IsValid())+
                      " voices="+std::to_string(voices_.size()));continue;
            }
            Voice v;v.origin=p.origin;
            v.sampleCount=wi::audio::GetSampleInfo(&sounds_[p.variant]).sample_count;
            if(!wi::audio::CreateSoundInstance(&sounds_[p.variant],&v.instance)) {
                Trace("INSTANCE_FAIL variant="+std::to_string(p.variant));continue;
            }
            const auto pos=Spatial(v.origin,listener,front);
            if(!std::getenv("RENEGADE_G4_THUNDER_NONSPATIAL")) wi::audio::Update3D(&v.instance,pos);
            wi::audio::SetVolume(p.volume*DistantGain(v.origin,listener)*level_[p.variant],&v.instance);
            wi::audio::Play(&v.instance);
            Trace("PLAY variant="+std::to_string(p.variant)+
                  " gain="+std::to_string(p.volume*DistantGain(v.origin,listener)*level_[p.variant]));
            voices_.push_back(std::move(v));
        }
        for(auto& v:voices_) {
            v.age+=dt;
            const auto samples=wi::audio::GetTotalSamplesPlayed(&v.instance);
            if(samples>0 && !v.started){v.started=true;Trace("SAMPLE_PROGRESS samples="+std::to_string(samples)+" master="+std::to_string(wi::audio::GetVolume())+" queued="+std::to_string(wi::audio::GetQueuedBufferCount(&v.instance))+" sfx="+std::to_string(wi::audio::GetSubmixVolume(wi::audio::SUBMIX_TYPE_SOUNDEFFECT)));}
            const auto pos=Spatial(v.origin,listener,front);
            if(!std::getenv("RENEGADE_G4_THUNDER_NONSPATIAL")) wi::audio::Update3D(&v.instance,pos);
        }
        voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](Voice& v){const auto samples=wi::audio::GetTotalSamplesPlayed(&v.instance);
            // XAudio2's callback-owned ended flag is initially true until
            // OnBufferStart. Never retire a freshly submitted voice by it.
            // GetState counts the submitted buffer before OnBufferStart and
            // reaches zero only after consumption, avoiding both that race
            // and metadata/decoded-sample rounding differences.
            const bool done=wi::audio::GetQueuedBufferCount(&v.instance)==0;
            if(v.age>10.0f && !done)Trace("VOICE_TIMEOUT samples="+std::to_string(samples));
            return done || v.age>10.0f;}),voices_.end());
    }
    std::size_t VoiceCount() const noexcept{return voices_.size();}
    std::uint64_t SamplesPlayed() const{std::uint64_t n=0;for(const auto& v:voices_)n+=wi::audio::GetTotalSamplesPlayed(&v.instance);return n;}
    void Reset(){pending_.clear();for(auto& v:voices_)wi::audio::Stop(&v.instance);voices_.clear();}
};
inline RuntimeThunderAudio& ThunderAudio(){static RuntimeThunderAudio service;return service;}
}
