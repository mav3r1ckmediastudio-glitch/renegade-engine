#pragma once

#include <Windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <evr.h>
#include <wrl/client.h>

#include <string>

namespace renegade::studio
{
    class StartupMediaFoundationPlayer final
    {
    public:
        static constexpr LONGLONG FadeDurationHundredNanoseconds = 4'500'000;

        StartupMediaFoundationPlayer() = default;
        ~StartupMediaFoundationPlayer();

        StartupMediaFoundationPlayer(const StartupMediaFoundationPlayer&) = delete;
        StartupMediaFoundationPlayer& operator=(const StartupMediaFoundationPlayer&) = delete;

        bool Start(HWND window, const std::wstring& mediaPath);
        void Pump();
        void Resize();
        void Repaint();
        void Shutdown(bool keepFadeOverlay = false);
        void ReleaseFadeOverlay();

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsFinished() const noexcept { return finished_; }
        [[nodiscard]] bool HasFailed() const noexcept { return failed_; }
        [[nodiscard]] bool HasVideo() const noexcept { return videoDisplay_ != nullptr; }
        [[nodiscard]] const std::string& FailureReason() const noexcept { return failureReason_; }

    private:
        bool CreatePlaybackTopology();
        bool AddTopologyBranch(
            IMFTopology* topology,
            IMFPresentationDescriptor* presentation,
            IMFStreamDescriptor* stream,
            IMFActivate* renderer);
        void HandleEvent(IMFMediaEvent* event);
        void BeginPlayback();
        void UpdateFade();
        void Fail(std::string reason, HRESULT result = S_OK);
        void EnsureFadeOverlay();
        void UpdateFadeOverlayBounds();
        void SetFadeAmount(float amount);
        static std::string FormatHRESULT(HRESULT result);

        HWND window_ = nullptr;
        HWND fadeOverlay_ = nullptr;
        std::wstring mediaPath_;

        bool comInitializedByUs_ = false;
        bool mediaFoundationStarted_ = false;
        bool active_ = false;
        bool topologyReady_ = false;
        bool playbackStarted_ = false;
        bool finished_ = false;
        bool failed_ = false;
        bool hasVideoStream_ = false;
        bool hasAudioStream_ = false;
        float fadeAmount_ = 0.0f;
        LONGLONG duration_ = 0;
        std::string failureReason_;

        Microsoft::WRL::ComPtr<IMFMediaSession> session_;
        Microsoft::WRL::ComPtr<IMFMediaSource> source_;
        Microsoft::WRL::ComPtr<IMFVideoDisplayControl> videoDisplay_;
        Microsoft::WRL::ComPtr<IMFSimpleAudioVolume> audioVolume_;
        Microsoft::WRL::ComPtr<IMFPresentationClock> clock_;
    };
}
