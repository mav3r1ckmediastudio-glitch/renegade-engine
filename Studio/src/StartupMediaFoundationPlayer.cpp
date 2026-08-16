#include "StartupMediaFoundationPlayer.h"

#include <mferror.h>
#include <propvarutil.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr wchar_t FadeOverlayClassName[] = L"RenegadeStartupFadeOverlay";

    LRESULT CALLBACK FadeOverlayWindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool EnsureFadeOverlayWindowClass() noexcept
    {
        static const bool registered = []() {
            WNDCLASSEXW windowClass = {};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = FadeOverlayWindowProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hbrBackground =
                reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            windowClass.lpszClassName = FadeOverlayClassName;

            if (RegisterClassExW(&windowClass) != 0)
                return true;

            return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }();
        return registered;
    }
}

namespace renegade::studio
{
    StartupMediaFoundationPlayer::~StartupMediaFoundationPlayer()
    {
        Shutdown(false);
    }

    bool StartupMediaFoundationPlayer::Start(
        const HWND window,
        const std::wstring& mediaPath)
    {
        Shutdown(false);

        window_ = window;
        mediaPath_ = mediaPath;
        finished_ = false;
        failed_ = false;
        active_ = false;
        topologyReady_ = false;
        playbackStarted_ = false;
        hasVideoStream_ = false;
        hasAudioStream_ = false;
        fadeAmount_ = 0.0f;
        duration_ = 0;
        failureReason_.clear();

        if (window_ == nullptr || !IsWindow(window_))
        {
            Fail("Media Foundation reveal received an invalid window handle");
            return false;
        }

        if (mediaPath_.empty() || !std::filesystem::exists(mediaPath_))
        {
            Fail("startup reveal MP4 is missing");
            return false;
        }

        HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(result))
        {
            comInitializedByUs_ = true;
        }
        else if (result != RPC_E_CHANGED_MODE)
        {
            Fail("COM initialization failed", result);
            return false;
        }

        result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
        if (FAILED(result))
        {
            Fail("Media Foundation initialization failed", result);
            return false;
        }
        mediaFoundationStarted_ = true;

        result = MFCreateMediaSession(nullptr, &session_);
        if (FAILED(result))
        {
            Fail("Media Foundation media session creation failed", result);
            return false;
        }

        ComPtr<IMFSourceResolver> resolver;
        result = MFCreateSourceResolver(&resolver);
        if (FAILED(result))
        {
            Fail("Media Foundation source resolver creation failed", result);
            return false;
        }

        MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
        ComPtr<IUnknown> sourceObject;
        result = resolver->CreateObjectFromURL(
            mediaPath_.c_str(),
            MF_RESOLUTION_MEDIASOURCE,
            nullptr,
            &objectType,
            &sourceObject);
        if (FAILED(result))
        {
            Fail("Media Foundation could not open the startup MP4", result);
            return false;
        }

        result = sourceObject.As(&source_);
        if (FAILED(result) || source_ == nullptr)
        {
            Fail("startup MP4 did not resolve to a Media Foundation source", result);
            return false;
        }

        if (!CreatePlaybackTopology())
            return false;

        active_ = true;
        return true;
    }

    bool StartupMediaFoundationPlayer::CreatePlaybackTopology()
    {
        ComPtr<IMFPresentationDescriptor> presentation;
        HRESULT result = source_->CreatePresentationDescriptor(&presentation);
        if (FAILED(result))
        {
            Fail("startup MP4 presentation descriptor creation failed", result);
            return false;
        }

        UINT64 duration = 0;
        if (SUCCEEDED(presentation->GetUINT64(MF_PD_DURATION, &duration)))
        {
            duration_ = static_cast<LONGLONG>(duration);
        }

        ComPtr<IMFTopology> topology;
        result = MFCreateTopology(&topology);
        if (FAILED(result))
        {
            Fail("Media Foundation topology creation failed", result);
            return false;
        }

        DWORD streamCount = 0;
        result = presentation->GetStreamDescriptorCount(&streamCount);
        if (FAILED(result))
        {
            Fail("startup MP4 stream enumeration failed", result);
            return false;
        }

        for (DWORD streamIndex = 0; streamIndex < streamCount; ++streamIndex)
        {
            BOOL selected = FALSE;
            ComPtr<IMFStreamDescriptor> stream;
            result = presentation->GetStreamDescriptorByIndex(
                streamIndex,
                &selected,
                &stream);
            if (FAILED(result))
            {
                Fail("startup MP4 stream descriptor lookup failed", result);
                return false;
            }
            if (!selected)
                continue;

            ComPtr<IMFMediaTypeHandler> handler;
            result = stream->GetMediaTypeHandler(&handler);
            if (FAILED(result))
            {
                Fail("startup MP4 media type lookup failed", result);
                return false;
            }

            GUID majorType = GUID_NULL;
            result = handler->GetMajorType(&majorType);
            if (FAILED(result))
            {
                Fail("startup MP4 major media type lookup failed", result);
                return false;
            }

            ComPtr<IMFActivate> renderer;
            if (majorType == MFMediaType_Video && !hasVideoStream_)
            {
                result = MFCreateVideoRendererActivate(window_, &renderer);
                hasVideoStream_ = SUCCEEDED(result);
            }
            else if (majorType == MFMediaType_Audio && !hasAudioStream_)
            {
                result = MFCreateAudioRendererActivate(&renderer);
                hasAudioStream_ = SUCCEEDED(result);
            }
            else
            {
                presentation->DeselectStream(streamIndex);
                continue;
            }

            if (FAILED(result) || renderer == nullptr)
            {
                Fail("startup reveal renderer activation failed", result);
                return false;
            }

            if (!AddTopologyBranch(
                    topology.Get(),
                    presentation.Get(),
                    stream.Get(),
                    renderer.Get()))
            {
                return false;
            }
        }

        if (!hasVideoStream_)
        {
            Fail("startup MP4 contains no playable video stream");
            return false;
        }
        if (!hasAudioStream_)
        {
            Fail("startup MP4 contains no playable audio stream");
            return false;
        }

        result = session_->SetTopology(0, topology.Get());
        if (FAILED(result))
        {
            Fail("Media Foundation rejected the startup playback topology", result);
            return false;
        }

        return true;
    }

    bool StartupMediaFoundationPlayer::AddTopologyBranch(
        IMFTopology* topology,
        IMFPresentationDescriptor* presentation,
        IMFStreamDescriptor* stream,
        IMFActivate* renderer)
    {
        if (topology == nullptr || presentation == nullptr ||
            stream == nullptr || renderer == nullptr)
        {
            Fail("startup playback topology received a null component");
            return false;
        }

        ComPtr<IMFTopologyNode> sourceNode;
        HRESULT result = MFCreateTopologyNode(
            MF_TOPOLOGY_SOURCESTREAM_NODE,
            &sourceNode);
        if (FAILED(result))
        {
            Fail("startup playback source-node creation failed", result);
            return false;
        }

        result = sourceNode->SetUnknown(MF_TOPONODE_SOURCE, source_.Get());
        if (SUCCEEDED(result))
            result = sourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presentation);
        if (SUCCEEDED(result))
            result = sourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, stream);
        if (SUCCEEDED(result))
            result = topology->AddNode(sourceNode.Get());
        if (FAILED(result))
        {
            Fail("startup playback source-node configuration failed", result);
            return false;
        }

        ComPtr<IMFTopologyNode> outputNode;
        result = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &outputNode);
        if (SUCCEEDED(result))
            result = outputNode->SetObject(renderer);
        if (SUCCEEDED(result))
            result = outputNode->SetUINT32(MF_TOPONODE_STREAMID, 0);
        if (SUCCEEDED(result))
            result = outputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
        if (SUCCEEDED(result))
            result = topology->AddNode(outputNode.Get());
        if (SUCCEEDED(result))
            result = sourceNode->ConnectOutput(0, outputNode.Get(), 0);
        if (FAILED(result))
        {
            Fail("startup playback output-node configuration failed", result);
            return false;
        }

        return true;
    }

    void StartupMediaFoundationPlayer::Pump()
    {
        if (!active_ || session_ == nullptr || failed_ || finished_)
            return;

        for (;;)
        {
            ComPtr<IMFMediaEvent> event;
            const HRESULT result =
                session_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &event);
            if (result == MF_E_NO_EVENTS_AVAILABLE)
                break;
            if (FAILED(result))
            {
                Fail("Media Foundation event polling failed", result);
                return;
            }
            HandleEvent(event.Get());
            if (failed_ || finished_)
                break;
        }

        if (playbackStarted_ && !failed_ && !finished_)
        {
            UpdateFade();
        }
    }

    void StartupMediaFoundationPlayer::HandleEvent(IMFMediaEvent* event)
    {
        if (event == nullptr)
            return;

        MediaEventType type = MEUnknown;
        HRESULT result = event->GetType(&type);
        if (FAILED(result))
        {
            Fail("Media Foundation event type lookup failed", result);
            return;
        }

        HRESULT eventStatus = S_OK;
        result = event->GetStatus(&eventStatus);
        if (FAILED(result))
        {
            Fail("Media Foundation event status lookup failed", result);
            return;
        }
        if (FAILED(eventStatus))
        {
            Fail("Media Foundation reported a playback failure", eventStatus);
            return;
        }

        if (type == MESessionTopologyStatus)
        {
            UINT32 status = 0;
            result = event->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status);
            if (FAILED(result))
            {
                Fail("Media Foundation topology-status lookup failed", result);
                return;
            }
            if (status == MF_TOPOSTATUS_READY && !topologyReady_)
            {
                topologyReady_ = true;
                BeginPlayback();
            }
            return;
        }

        if (type == MEEndOfPresentation || type == MESessionEnded)
        {
            if (audioVolume_ != nullptr)
            {
                audioVolume_->SetMasterVolume(0.0f);
            }
            SetFadeAmount(1.0f);
            finished_ = true;
            active_ = false;
        }
    }

    void StartupMediaFoundationPlayer::BeginPlayback()
    {
        HRESULT result = MFGetService(
            session_.Get(),
            MR_VIDEO_RENDER_SERVICE,
            IID_PPV_ARGS(videoDisplay_.ReleaseAndGetAddressOf()));
        if (FAILED(result) || videoDisplay_ == nullptr)
        {
            Fail("Media Foundation could not obtain EVR display control", result);
            return;
        }

        videoDisplay_->SetBorderColor(RGB(0, 0, 0));
        videoDisplay_->SetAspectRatioMode(MFVideoARMode_PreservePicture);
        Resize();

        result = MFGetService(
            session_.Get(),
            MR_POLICY_VOLUME_SERVICE,
            IID_PPV_ARGS(audioVolume_.ReleaseAndGetAddressOf()));
        if (FAILED(result) || audioVolume_ == nullptr)
        {
            Fail("Media Foundation could not obtain startup audio volume control", result);
            return;
        }
        audioVolume_->SetMasterVolume(1.0f);

        PROPVARIANT startPosition;
        PropVariantInit(&startPosition);
        result = session_->Start(&GUID_NULL, &startPosition);
        PropVariantClear(&startPosition);
        if (FAILED(result))
        {
            Fail("Media Foundation could not start startup playback", result);
            return;
        }

        playbackStarted_ = true;
        ComPtr<IMFClock> sessionClock;
        if (SUCCEEDED(session_->GetClock(&sessionClock)) && sessionClock != nullptr)
        {
            (void)sessionClock.As(&clock_);
        }
    }

    void StartupMediaFoundationPlayer::UpdateFade()
    {
        if (duration_ <= FadeDurationHundredNanoseconds)
            return;

        if (clock_ == nullptr)
        {
            ComPtr<IMFClock> sessionClock;
            if (SUCCEEDED(session_->GetClock(&sessionClock)) && sessionClock != nullptr)
            {
                (void)sessionClock.As(&clock_);
            }
            if (clock_ == nullptr)
                return;
        }

        MFTIME presentationTime = 0;
        if (FAILED(clock_->GetTime(&presentationTime)))
            return;

        const LONGLONG fadeStart =
            duration_ - FadeDurationHundredNanoseconds;
        if (presentationTime < fadeStart)
            return;

        const double normalized = std::clamp(
            static_cast<double>(presentationTime - fadeStart) /
                static_cast<double>(FadeDurationHundredNanoseconds),
            0.0,
            1.0);
        const float amount = static_cast<float>(normalized);
        SetFadeAmount(amount);
        if (audioVolume_ != nullptr)
        {
            audioVolume_->SetMasterVolume(1.0f - amount);
        }
    }

    void StartupMediaFoundationPlayer::Resize()
    {
        if (window_ == nullptr)
            return;

        if (videoDisplay_ != nullptr)
        {
            RECT client = {};
            if (GetClientRect(window_, &client))
            {
                (void)videoDisplay_->SetVideoPosition(nullptr, &client);
            }
        }
        UpdateFadeOverlayBounds();
    }

    void StartupMediaFoundationPlayer::Repaint()
    {
        if (videoDisplay_ != nullptr)
        {
            (void)videoDisplay_->RepaintVideo();
        }
    }

    void StartupMediaFoundationPlayer::EnsureFadeOverlay()
    {
        if (fadeOverlay_ != nullptr || window_ == nullptr)
            return;
        if (!EnsureFadeOverlayWindowClass())
            return;

        fadeOverlay_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            FadeOverlayClassName,
            L"",
            WS_POPUP,
            0,
            0,
            1,
            1,
            window_,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (fadeOverlay_ == nullptr)
            return;

        SetLayeredWindowAttributes(fadeOverlay_, 0, 0, LWA_ALPHA);
        UpdateFadeOverlayBounds();
        ShowWindow(fadeOverlay_, SW_SHOWNOACTIVATE);
    }

    void StartupMediaFoundationPlayer::UpdateFadeOverlayBounds()
    {
        if (fadeOverlay_ == nullptr || window_ == nullptr)
            return;

        RECT client = {};
        if (!GetClientRect(window_, &client))
            return;

        POINT topLeft = { client.left, client.top };
        POINT bottomRight = { client.right, client.bottom };
        if (!ClientToScreen(window_, &topLeft) ||
            !ClientToScreen(window_, &bottomRight))
        {
            return;
        }

        SetWindowPos(
            fadeOverlay_,
            HWND_TOP,
            topLeft.x,
            topLeft.y,
            std::max<LONG>(1, bottomRight.x - topLeft.x),
            std::max<LONG>(1, bottomRight.y - topLeft.y),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void StartupMediaFoundationPlayer::SetFadeAmount(const float amount)
    {
        fadeAmount_ = std::clamp(amount, 0.0f, 1.0f);
        if (fadeAmount_ <= 0.0f)
            return;

        EnsureFadeOverlay();
        if (fadeOverlay_ != nullptr)
        {
            const BYTE alpha = static_cast<BYTE>(fadeAmount_ * 255.0f + 0.5f);
            SetLayeredWindowAttributes(fadeOverlay_, 0, alpha, LWA_ALPHA);
        }
    }

    void StartupMediaFoundationPlayer::Shutdown(const bool keepFadeOverlay)
    {
        if (session_ != nullptr)
        {
            (void)session_->Stop();
            (void)session_->Close();

            for (int attempt = 0; attempt < 250; ++attempt)
            {
                ComPtr<IMFMediaEvent> event;
                const HRESULT result =
                    session_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &event);
                if (result == MF_E_NO_EVENTS_AVAILABLE)
                {
                    Sleep(1);
                    continue;
                }
                if (FAILED(result) || event == nullptr)
                    break;

                MediaEventType type = MEUnknown;
                if (SUCCEEDED(event->GetType(&type)) && type == MESessionClosed)
                    break;
            }
        }

        if (source_ != nullptr)
        {
            (void)source_->Shutdown();
        }
        if (session_ != nullptr)
        {
            (void)session_->Shutdown();
        }

        clock_.Reset();
        audioVolume_.Reset();
        videoDisplay_.Reset();
        source_.Reset();
        session_.Reset();

        if (mediaFoundationStarted_)
        {
            MFShutdown();
            mediaFoundationStarted_ = false;
        }
        if (comInitializedByUs_)
        {
            CoUninitialize();
            comInitializedByUs_ = false;
        }

        active_ = false;
        topologyReady_ = false;
        playbackStarted_ = false;

        if (!keepFadeOverlay)
        {
            ReleaseFadeOverlay();
        }
    }

    void StartupMediaFoundationPlayer::ReleaseFadeOverlay()
    {
        if (fadeOverlay_ != nullptr)
        {
            DestroyWindow(fadeOverlay_);
            fadeOverlay_ = nullptr;
        }
    }

    void StartupMediaFoundationPlayer::Fail(
        std::string reason,
        const HRESULT result)
    {
        if (result != S_OK)
        {
            reason += " // " + FormatHRESULT(result);
        }
        failureReason_ = std::move(reason);
        failed_ = true;
        finished_ = true;
        active_ = false;
    }

    std::string StartupMediaFoundationPlayer::FormatHRESULT(
        const HRESULT result)
    {
        std::ostringstream stream;
        stream << "HRESULT=0x"
               << std::uppercase
               << std::hex
               << std::setw(8)
               << std::setfill('0')
               << static_cast<unsigned long>(result);
        return stream.str();
    }
}
