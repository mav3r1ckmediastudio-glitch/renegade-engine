#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <string>

namespace renegade::studio
{
    // Gate 2A owns only the cinematic logo reveal. It deliberately knows
    // nothing about developer identity, the Identity Handshake or Project Hub.
    // Those remain later owner-gated stages.
    class StartupRevealRenderPath final : public wi::RenderPath
    {
    public:
        static constexpr float FadeOutSeconds = 0.45f;

        void Configure(std::string videoPath, std::string audioPath)
        {
            videoPath_ = std::move(videoPath);
            audioPath_ = std::move(audioPath);
        }

        void Start() override
        {
            ResetState();

            if (videoPath_.empty() || !wi::helper::FileExists(videoPath_))
            {
                FailOpen("startup reveal video is missing: " + videoPath_);
                return;
            }
            if (audioPath_.empty() || !wi::helper::FileExists(audioPath_))
            {
                FailOpen("startup reveal companion audio is missing: " + audioPath_);
                return;
            }

            if (!wi::video::CreateVideo(videoPath_, &video_) || !video_.IsValid())
            {
                FailOpen("startup reveal video could not be decoded: " + videoPath_);
                return;
            }
            if (video_.duration_seconds <= 0.0f || video_.width == 0 || video_.height == 0)
            {
                FailOpen("startup reveal video metadata is invalid: " + videoPath_);
                return;
            }
            if (!wi::video::CreateVideoInstance(&video_, &videoInstance_) ||
                !videoInstance_.IsValid())
            {
                // Wicked's pinned video path is GPU decode only. Unsupported
                // H.264 decode capability must never prevent Studio startup.
                FailOpen("startup reveal H.264 decoder is unavailable");
                return;
            }

            if (!wi::audio::CreateSound(audioPath_, &audio_) || !audio_.IsValid())
            {
                FailOpen("startup reveal companion audio could not be loaded: " + audioPath_);
                return;
            }
            audioInstance_.type = wi::audio::SUBMIX_TYPE_SOUNDEFFECT;
            if (!wi::audio::CreateSoundInstance(&audio_, &audioInstance_) ||
                !audioInstance_.IsValid())
            {
                FailOpen("startup reveal companion audio instance could not be created");
                return;
            }

            ready_ = true;
            wi::backlog::post("[PR58-GATE2A] Startup reveal ready: " + videoPath_);
        }

        void Stop() override
        {
            if (audioInstance_.IsValid())
            {
                wi::audio::Stop(&audioInstance_);
                wi::audio::SetVolume(1.0f, &audioInstance_);
            }
            videoInstance_.flags &= ~wi::video::VideoInstance::Flags::Playing;
        }

        void Update(float dt) override
        {
            if (!ready_ || finished_ || failed_)
                return;

            if (!playbackStarted_)
            {
                // Start both clocks from the same Studio update boundary.
                videoInstance_.flags |= wi::video::VideoInstance::Flags::Playing;
                wi::audio::SetVolume(1.0f, &audioInstance_);
                wi::audio::Play(&audioInstance_);
                playbackStarted_ = true;
            }

            wi::video::UpdateVideo(&videoInstance_, dt);

            const float duration = video_.duration_seconds;
            const float current = std::clamp(videoInstance_.current_time, 0.0f, duration);
            const float fadeStart = std::max(0.0f, duration - FadeOutSeconds);
            if (current >= fadeStart)
            {
                const float fadeDuration = std::max(0.001f, duration - fadeStart);
                fadeToBlack_ = std::clamp((current - fadeStart) / fadeDuration, 0.0f, 1.0f);
                wi::audio::SetVolume(1.0f - fadeToBlack_, &audioInstance_);
            }

            if (videoInstance_.current_time >= duration)
            {
                fadeToBlack_ = 1.0f;
                wi::audio::Stop(&audioInstance_);
                wi::audio::SetVolume(1.0f, &audioInstance_);
                videoInstance_.flags &= ~wi::video::VideoInstance::Flags::Playing;
                finished_ = true;
                wi::backlog::post("[PR58-GATE2A] Startup reveal completed");
            }
        }

        void Render() const override
        {
            if (!ready_ || failed_ || finished_)
                return;

            auto* device = wi::graphics::GetDevice();
            if (device == nullptr)
                return;

            const wi::graphics::CommandList cmd = device->BeginCommandList();
            if (wi::video::IsDecodingRequired(&videoInstance_))
            {
                wi::video::DecodeVideo(&videoInstance_, cmd);
            }
            // Resolve is safe to call each frame and internally consumes any
            // decoder output waiting to become a displayable RGB texture.
            wi::video::ResolveVideoToRGB(&videoInstance_, cmd);
        }

        void Compose(wi::graphics::CommandList cmd) const override
        {
            wi::image::SetCanvas(*this);

            // Always own the entire backbuffer. This prevents the Studio editor
            // or Project Hub from bleeding through around a 16:9 reveal.
            wi::image::Params background;
            background.enableFullScreen();
            background.blendFlag = wi::enums::BLENDMODE_OPAQUE;
            wi::image::Draw(wi::texturehelper::getBlack(), background, cmd);

            if (!ready_ || failed_)
                return;

            const wi::graphics::Texture& frame = videoInstance_.output.texture;
            if (!frame.IsValid())
                return;

            const float canvasWidth = GetLogicalWidth();
            const float canvasHeight = GetLogicalHeight();
            if (canvasWidth <= 0.0f || canvasHeight <= 0.0f)
                return;

            const float videoAspect = static_cast<float>(video_.width) /
                static_cast<float>(video_.height);
            const float canvasAspect = canvasWidth / canvasHeight;

            float drawWidth = canvasWidth;
            float drawHeight = canvasHeight;
            if (canvasAspect > videoAspect)
            {
                drawWidth = canvasHeight * videoAspect;
            }
            else
            {
                drawHeight = canvasWidth / videoAspect;
            }

            wi::image::Params reveal;
            reveal.pos = XMFLOAT3(canvasWidth * 0.5f, canvasHeight * 0.5f, 0.0f);
            reveal.siz = XMFLOAT2(drawWidth, drawHeight);
            reveal.pivot = XMFLOAT2(0.5f, 0.5f);
            reveal.blendFlag = wi::enums::BLENDMODE_ALPHA;
            reveal.opacity = 1.0f - fadeToBlack_;
            reveal.image_subresource = videoInstance_.GetCurrentFrameTextureSRGBSubresource();
            wi::image::Draw(&frame, reveal, cmd);
        }

        [[nodiscard]] bool IsFinished() const noexcept
        {
            return finished_;
        }

        [[nodiscard]] bool HasFailedOpen() const noexcept
        {
            return failed_;
        }

        [[nodiscard]] const std::string& FailureReason() const noexcept
        {
            return failureReason_;
        }

    private:
        void ResetState()
        {
            ready_ = false;
            playbackStarted_ = false;
            finished_ = false;
            failed_ = false;
            fadeToBlack_ = 0.0f;
            failureReason_.clear();
            video_ = {};
            videoInstance_ = {};
            audio_ = {};
            audioInstance_ = {};
        }

        void FailOpen(std::string reason)
        {
            failed_ = true;
            finished_ = true;
            failureReason_ = std::move(reason);
            wi::backlog::post("[PR58-GATE2A] Reveal skipped safely: " + failureReason_);
        }

        std::string videoPath_;
        std::string audioPath_;
        wi::video::Video video_;
        mutable wi::video::VideoInstance videoInstance_;
        wi::audio::Sound audio_;
        wi::audio::SoundInstance audioInstance_;
        bool ready_ = false;
        bool playbackStarted_ = false;
        bool finished_ = false;
        bool failed_ = false;
        float fadeToBlack_ = 0.0f;
        std::string failureReason_;
    };
}
