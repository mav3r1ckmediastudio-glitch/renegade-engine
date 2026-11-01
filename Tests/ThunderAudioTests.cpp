#include "../Runtime/src/RuntimeThunderAudio.h"
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
// Actual default render endpoint loopback, not a substitute WAV backend.
struct Loopback {
    std::atomic<bool> stop{false};
    std::thread worker;
    double peak=0,energy=0;
    uint64_t samples=0;
    HRESULT result=E_FAIL;
    void Start(){worker=std::thread([this]{
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        ComPtr<IMMDeviceEnumerator> devices;ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX* fmt=nullptr;
        result=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices));
        if(FAILED(result))return;
        result=devices->GetDefaultAudioEndpoint(eRender,eConsole,&device);if(FAILED(result))return;
        LPWSTR id=nullptr;device->GetId(&id);std::wcout<<L"LOOPBACK_ENDPOINT "<<(id?id:L"")<<L"\n";CoTaskMemFree(id);
        result=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)&client);if(FAILED(result))return;
        result=client->GetMixFormat(&fmt);if(FAILED(result))return;
        result=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,10000000,0,fmt,nullptr);
        if(FAILED(result)){CoTaskMemFree(fmt);return;}
        result=client->GetService(IID_PPV_ARGS(&capture));if(FAILED(result)){CoTaskMemFree(fmt);return;}
        client->Start();
        const bool fp=fmt->wFormatTag==WAVE_FORMAT_IEEE_FLOAT ||
            (fmt->wFormatTag==WAVE_FORMAT_EXTENSIBLE &&
             ((WAVEFORMATEXTENSIBLE*)fmt)->SubFormat.Data1==WAVE_FORMAT_IEEE_FLOAT);
        std::cout<<"LOOPBACK_FORMAT rate="<<fmt->nSamplesPerSec<<" channels="<<fmt->nChannels<<" bits="<<fmt->wBitsPerSample<<" float="<<fp<<"\n";
        while(!stop){
            UINT32 available=0;capture->GetNextPacketSize(&available);
            while(available){
                BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
                if(FAILED(capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr)))break;
                if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT) && data){
                    for(size_t i=0;i<size_t(frames)*fmt->nChannels;++i){
                        double x=0;
                        if(fp && fmt->wBitsPerSample==32)x=((float*)data)[i];
                        else if(fmt->wBitsPerSample==16)x=((short*)data)[i]/32768.;
                        peak=std::max(peak,std::abs(x));energy+=x*x;++samples;
                    }
                }
                capture->ReleaseBuffer(frames);capture->GetNextPacketSize(&available);
            }
            Sleep(4);
        }
        client->Stop();CoTaskMemFree(fmt);result=S_OK;
        // COM objects are released on this thread before process exit.
    });}
    void Finish(){stop=true;if(worker.joinable())worker.join();}
    ~Loopback(){Finish();}
};
int main(int argc,char** argv){
    if(argc>1 && std::string(argv[1])=="--capture-only"){
        const int seconds=argc>2?std::clamp(std::atoi(argv[2]),1,50):25;
        Loopback measured;measured.Start();Sleep(seconds*1000);measured.Finish();
        std::cout<<"CAPTURE_ONLY seconds="<<seconds<<" peak="<<measured.peak
                 <<" rms="<<sqrt(measured.energy/std::max<uint64_t>(1,measured.samples))
                 <<" hr="<<measured.result<<"\n";
        return FAILED(measured.result)?8:0;
    }
    if(argc<2){std::cerr<<"Supply absolute source-worktree path\n";return 2;}
    SetCurrentDirectoryA(argv[1]);
    wi::audio::Initialize();
    wi::audio::SetVolume(1);
    wi::audio::SetSubmixVolume(wi::audio::SUBMIX_TYPE_SOUNDEFFECT,1);
    // First prove an immediate non-spatial WAV on the real backend.
    Loopback loop;loop.Start();Sleep(400);
    wi::audio::Sound sound;wi::audio::SoundInstance instance;
    if(!wi::audio::CreateSound("Runtime/Content/weather/thunder/default3d/thunder_booming_01.wav",&sound) ||
       !wi::audio::CreateSoundInstance(&sound,&instance))return 3;
    wi::audio::SetVolume(.65f,&instance);wi::audio::Play(&instance);
    uint64_t maximum=0;
    for(int i=0;i<500;++i){maximum=std::max(maximum,wi::audio::GetTotalSamplesPlayed(&instance));Sleep(10);}
    loop.Finish();
    std::cout<<"DIRECT buffers_queued="<<wi::audio::GetQueuedBufferCount(&instance)<<" samples="<<maximum<<" loopback_peak="<<loop.peak<<" loopback_rms="<<sqrt(loop.energy/std::max<uint64_t>(1,loop.samples))<<" hr="<<loop.result<<"\n";
    if(maximum==0 || FAILED(loop.result) || loop.peak<.0001)return 4;
    // Exercise the production service in the same frame as submission.
    _putenv_s("RENEGADE_G4_THUNDER_NONSPATIAL","1");
    renegade::runtime::RuntimeThunderAudio thunder;
    const DirectX::XMFLOAT3 origin{0,0,1},listener{0,0,0},front{0,0,1};
    for(int iteration=0;iteration<100;++iteration){
        thunder.Queue(origin,listener,true,.6f,false,0);
        thunder.Update(.001f,listener,front);
        if(thunder.VoiceCount()!=1){std::cerr<<"Voice retired before start iteration="<<iteration;return 5;}
        Sleep(12);thunder.Update(.012f,listener,front);
        thunder.Reset();
    }
    _putenv_s("RENEGADE_G4_THUNDER_NONSPATIAL","");
    Loopback spatial;spatial.Start();Sleep(300);
    thunder.Queue({4000,0,4000},listener,true,.7f,false,1);
    uint64_t progress=0;
    for(int i=0;i<500;++i){thunder.Update(.01f,listener,front);progress=std::max(progress,thunder.SamplesPlayed());Sleep(10);}
    spatial.Finish();
    std::cout<<"SPATIAL samples="<<progress<<" peak="<<spatial.peak<<" voices_remaining="<<thunder.VoiceCount()<<"\n";
    if(!progress || spatial.peak<.0001 || thunder.VoiceCount()!=0)return 6;
    if(argc>2){
        // Real scene-scale listener coordinates and pitched/rotated directions.
        const DirectX::XMFLOAT3 eye{150,35,250};
        for(int variant=0;variant<9;++variant){
            const DirectX::XMFLOAT3 directions[3]={{0,0,1},{1,0,0},{0,.99995f,.01f}};
            const auto direction=directions[variant%3];
            Loopback recorded;recorded.Start();Sleep(250);
            thunder.Queue({eye.x+4000,eye.y+500,eye.z+4000},eye,true,.85f,false,variant+2);
            uint64_t played=0;
            for(int tick=0;tick<500;++tick){
                thunder.Update(.01f,eye,direction);
                played=std::max(played,thunder.SamplesPlayed());Sleep(10);
            }
            recorded.Finish();
            std::cout<<"VARIANT "<<variant<<" samples="<<played<<" peak="<<recorded.peak
                     <<" remaining="<<thunder.VoiceCount()<<"\n";
            if(!played || recorded.peak<.0001 || thunder.VoiceCount()!=0)return 7;
        }
    }
    std::cout<<"PASS direct backend + loopback + 100 startup lifetimes + production spatial output\n";
    return 0;
}
