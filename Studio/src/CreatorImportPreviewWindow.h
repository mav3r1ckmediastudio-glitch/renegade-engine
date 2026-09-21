#pragma once

#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/StudioSession.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Decorative native shell behind the real stage controls. It is attached
    // last so Wicked renders it first; it never participates in hit testing.
    class CreatorImportInspectorShell final : public wi::gui::Widget
    {
    public:
        CreatorImportInspectorShell()
        {
            SetName("Importer Inspector Shell");
            SetShadowRadius(0.0f);
        }

        void SetBodyBounds(const float top, const float height) noexcept
        {
            bodyTop_ = top;
            bodyHeight_ = height;
        }

        void Render(const wi::Canvas&, const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;
            const float x = translation.x;
            const float y = translation.y;
            const float width = scale.x;
            const auto draw = [cmd](const float left, const float top,
                const float w, const float h, const wi::Color color)
            {
                if (w <= 0.0f || h <= 0.0f)
                    return;
                wi::image::Params params(left, top, w, h, color);
                params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                wi::image::Draw(nullptr, params, cmd);
            };

            // The title, status and help text remain genuine native labels.
            draw(x + 1.0f, y + 1.0f, width - 2.0f, 141.0f,
                wi::Color(13, 20, 26, 255));
            draw(x + 1.0f, y + 1.0f, 3.0f, 141.0f,
                wi::Color(209, 125, 73, 255));
            draw(x + 12.0f, y + 141.0f, width - 24.0f, 1.0f,
                wi::Color(54, 69, 80, 255));

            if (bodyHeight_ > 0.0f)
            {
                // A separate stage surface keeps editor-style controls from
                // appearing as loose rows underneath a highlighted heading.
                draw(x + 12.0f, y + bodyTop_, width - 24.0f, bodyHeight_,
                    wi::Color(26, 35, 43, 255));
                draw(x + 12.0f, y + bodyTop_, 2.0f, bodyHeight_,
                    wi::Color(161, 98, 60, 255));
            }
        }

    private:
        float bodyTop_ = 0.0f;
        float bodyHeight_ = 0.0f;
    };

    // The model importer is a presentation workspace, not part of the authored
    // level. Entering it therefore snapshots the scene's visible sky/fog state
    // and replaces only those presentation fields with a neutral backdrop.
    // Ambient light is deliberately left alone here because the existing
    // importer lighting transaction already snapshots/restores it.
    //
    // This class shadows Window::SetVisible() so every existing importer
    // enter/exit path (normal open, cancel and governed commit) gets the same
    // weather isolation without adding a second lifecycle to StudioRenderPath.
    class CreatorImportPreviewWindow final : public wi::gui::Window
    {
    public:
        // Caller owns this scene until after SetVisible(false).
        // Weather changes must never leak into the authored document.
        void SetPreviewScene(wi::scene::Scene* previewScene) noexcept
        {
            previewScene_ = previewScene;
        }

        void InitializeInspectorShell()
        {
            // Last child is rendered beneath the actual headings and inputs.
            AddWidget(&inspectorShell_);
            inspectorShell_.SetEnabled(false);
            inspectorShell_.priority_change = false;
        }

        void LayoutInspectorShell(const float width,
            const float bodyTop, const float bodyHeight)
        {
            inspectorShell_.SetPos(XMFLOAT2(0.0f, 0.0f));
            inspectorShell_.SetSize(XMFLOAT2(width, 142.0f));
            inspectorShell_.SetBodyBounds(bodyTop, bodyHeight);
        }

        void OffsetVisibleStageContent(const float offset)
        {
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget == nullptr || !widget->IsVisible() ||
                    widget->GetName().rfind("Importer Stage ", 0) == 0 ||
                    widget->GetName() == "MODEL IMPORTER // PREVIEW BEFORE COMMIT")
                    continue;
                if (widget == &label || widget->GetPos().y < 178.0f)
                    continue;
                const XMFLOAT2 position = widget->GetPos();
                widget->SetPos(XMFLOAT2(position.x, position.y + offset));
            }
        }

        void SetVisible(const bool visible)
        {
            const bool closingPreview = !visible && previewWeatherCaptured_;
            if (visible && !previewWeatherCaptured_)
            {
                CaptureAndNeutralizePreviewWeather();
            }
            else if (!visible && previewWeatherCaptured_)
            {
                RestorePreviewWeather();
            }

            wi::gui::Window::SetVisible(visible);
            if (closingPreview)
            {
                // DismissImportScalePanel refreshes the Inspector and then
                // makes its parent Window visible again. Wicked propagates that
                // visibility to the Window children, which can temporarily
                // reveal both Environment and Terrain specialist controls.
                // Re-submit the already-active workspace action here. The
                // Studio action queue processes it after the importer dismiss
                // action completes, exactly matching the user's manual heading
                // click that restores the correct specialist visibility.
                if (auto* chrome = CreatorAssetStudioChrome::Current())
                {
                    chrome->RequestCurrentWorkspaceReconcile();
                }
            }
        }

    private:
        struct WeatherPresentationState
        {
            bool valid = false;
            std::uint32_t flags = 0;
            XMFLOAT3 horizon = {};
            XMFLOAT3 zenith = {};
            float skyExposure = 1.0f;
            float fogDensity = 0.0f;
            float stars = 0.0f;
            std::string skyMapName;
            wi::Resource skyMap;
        };

        static WeatherPresentationState Capture(
            const wi::scene::WeatherComponent& weather)
        {
            WeatherPresentationState state;
            state.valid = true;
            state.flags = weather._flags;
            state.horizon = weather.horizon;
            state.zenith = weather.zenith;
            state.skyExposure = weather.skyExposure;
            state.fogDensity = weather.fogDensity;
            state.stars = weather.stars;
            state.skyMapName = weather.skyMapName;
            state.skyMap = weather.skyMap;
            return state;
        }

        static void ApplyNeutral(
            wi::scene::WeatherComponent& weather)
        {
            weather.SetRealisticSky(false);
            weather.SetVolumetricClouds(false);
            weather.SetHeightFog(false);
            weather.SetOverrideFogColor(false);
            weather.horizon = XMFLOAT3(0.065f, 0.070f, 0.075f);
            weather.zenith = XMFLOAT3(0.065f, 0.070f, 0.075f);
            weather.skyExposure = 1.0f;
            weather.fogDensity = 0.0f;
            weather.stars = 0.0f;
            weather.skyMapName.clear();
            weather.skyMap = {};
        }

        static void Restore(
            wi::scene::WeatherComponent& weather,
            const WeatherPresentationState& state)
        {
            if (!state.valid)
                return;

            weather._flags = state.flags;
            weather.horizon = state.horizon;
            weather.zenith = state.zenith;
            weather.skyExposure = state.skyExposure;
            weather.fogDensity = state.fogDensity;
            weather.stars = state.stars;
            weather.skyMapName = state.skyMapName;
            weather.skyMap = state.skyMap;
        }

        void CaptureAndNeutralizePreviewWeather()
        {
            auto* session = bridge::StudioSession::Current();
            if (session == nullptr)
                return;

            auto& scene = previewScene_ != nullptr
                ? *previewScene_ : session->Scenes().GetScene();
            sceneWeatherBefore_ = Capture(scene.weather);
            weatherEntity_ = previewScene_ != nullptr
                ? wi::ecs::INVALID_ENTITY : session->Scenes().WeatherEntity();
            entityWeatherBefore_ = {};
            if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
            {
                entityWeatherBefore_ = Capture(*weather);
            }

            ApplyNeutral(scene.weather);
            if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
            {
                ApplyNeutral(*weather);
            }
            previewWeatherCaptured_ = true;
        }

        void RestorePreviewWeather()
        {
            auto* session = bridge::StudioSession::Current();
            if (session != nullptr)
            {
                auto& scene = previewScene_ != nullptr
                    ? *previewScene_ : session->Scenes().GetScene();
                Restore(scene.weather, sceneWeatherBefore_);
                if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
                {
                    Restore(*weather, entityWeatherBefore_);
                }
            }

            previewWeatherCaptured_ = false;
            weatherEntity_ = wi::ecs::INVALID_ENTITY;
            sceneWeatherBefore_ = {};
            entityWeatherBefore_ = {};
        }

        CreatorImportInspectorShell inspectorShell_;
        wi::scene::Scene* previewScene_ = nullptr;
        bool previewWeatherCaptured_ = false;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        WeatherPresentationState sceneWeatherBefore_;
        WeatherPresentationState entityWeatherBefore_;
    };
}
