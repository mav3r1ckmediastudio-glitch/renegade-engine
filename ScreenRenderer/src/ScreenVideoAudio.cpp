#include "ScreenVideoAudio.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <shlwapi.h>
#include <wrl.h>
#include <limits>
#include <cmath>

namespace renegade::screen
{
    using Microsoft::WRL::ComPtr;
    class AudioEvents final : public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,IMFMediaEngineNotify>
    {
    public:
        HRESULT STDMETHODCALLTYPE EventNotify(DWORD event,DWORD_PTR arg,DWORD) override
        {
            if (event==MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE) SetEvent(reinterpret_cast<HANDLE>(arg));
            return S_OK;
        }
    };
    struct ScreenVideoAudio::Impl
    {
        ComPtr<IMFMediaEngineEx> engine;
        bool started=false;
        bool playing=false;
        ~Impl()
        {
            if (engine) engine->Shutdown();
            engine.Reset();
            if (started) MFShutdown();
        }
    };
    ScreenVideoAudio::ScreenVideoAudio():impl_(std::make_unique<Impl>()) {}
    ScreenVideoAudio::~ScreenVideoAudio()=default;
    bool ScreenVideoAudio::Open(const std::vector<std::uint8_t>& mp4)
    {
        if (mp4.empty() || mp4.size()>UINT_MAX || FAILED(MFStartup(MF_VERSION))) return false;
        impl_->started=true;
        ComPtr<IStream> memory;
        memory.Attach(SHCreateMemStream(mp4.data(),static_cast<UINT>(mp4.size())));
        ComPtr<IMFByteStream> bytes;
        ComPtr<IMFAttributes> attributes;
        ComPtr<IMFMediaEngineClassFactory> factory;
        ComPtr<IMFMediaEngine> engine;
        auto events=Microsoft::WRL::Make<AudioEvents>();
        if (!memory || !events || FAILED(MFCreateMFByteStreamOnStream(memory.Get(),&bytes)) ||
            FAILED(MFCreateAttributes(&attributes,1)) ||
            FAILED(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK,events.Get())) ||
            FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY,attributes.Get(),&engine)) ||
            FAILED(engine.As(&impl_->engine))) return false;
        BSTR name=SysAllocString(L"screen.mp4");
        const auto result=impl_->engine->SetSourceFromByteStream(bytes.Get(),name);
        SysFreeString(name);
        return SUCCEEDED(result);
    }
    void ScreenVideoAudio::Update(float seconds,bool active)
    {
        auto& audio=*impl_;
        if (!audio.engine || audio.engine->GetReadyState()<MF_MEDIA_ENGINE_READY_HAVE_CURRENT_DATA) return;
        if (!active)
        {
            if (audio.playing) audio.engine->Pause();
            audio.playing=false;
            return;
        }
        if (std::abs(audio.engine->GetCurrentTime()-seconds)>.25) audio.engine->SetCurrentTime(seconds);
        if (!audio.playing || audio.engine->IsPaused()) audio.playing=SUCCEEDED(audio.engine->Play());
    }
}
