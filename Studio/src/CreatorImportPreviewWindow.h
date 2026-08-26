#pragma once

#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "renegade/bridge/StudioSession.h"

namespace renegade::studio
{
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
        using HiddenCallback = std::function<void()>;

        CreatorImportPreviewWindow() = default;
        explicit CreatorImportPreviewWindow(HiddenCallback callback)
            : hiddenCallback_(std::move(callback))
        {
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

            // Import cancellation performs its command rollback after hiding
            // this window. Defer the host refresh to Wicked's safe point so it
            // runs after that rollback and after creatorModelImporter is reset.
            // This also gives successful import exits the same deterministic
            // Scene/Environment/Terrain visibility reconciliation.
            if (closingPreview && hiddenCallback_)
            {
                const HiddenCallback callback = hiddenCallback_;
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [callback](std::uint64_t)
                    {
                        callback();
                    });
            }
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            ReflowFinalImportPage();
            wi::gui::Window::Update(canvas, dt);
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

        void ReflowFinalImportPage()
        {
            // The final IMPORT page also shows Asset Name and Content/Models.
            // The old fixed thumbnail block began at y=178/214, physically
            // underneath those fields (190..262). Keep the accepted square
            // preview size but place the whole final block after the fields.
            constexpr float actionBarY = 276.0f;
            constexpr float previewY = 312.0f;

            float previewSide = 244.0f;
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget != nullptr &&
                    widget->GetName() == "Final Asset Thumbnail Preview")
                {
                    previewSide = std::max(1.0f, widget->GetSize().y);
                    break;
                }
            }

            const float captureY = previewY + previewSide + 10.0f;
            const float statusY = captureY + 48.0f;
            const float confirmY = captureY + 96.0f;
            const float cancelY = captureY + 150.0f;

            for (wi::gui::Widget* widget : widgets)
            {
                if (widget == nullptr)
                    continue;

                const std::string& name = widget->GetName();
                const XMFLOAT2 current = widget->GetPos();
                if (name == "THUMBNAIL & IMPORT")
                {
                    widget->SetPos(XMFLOAT2(current.x, actionBarY));
                }
                else if (name == "Final Asset Thumbnail Preview")
                {
                    widget->SetPos(XMFLOAT2(current.x, previewY));
                }
                else if (name == "Capture Asset Thumbnail")
                {
                    widget->SetPos(XMFLOAT2(current.x, captureY));
                }
                else if (name == "THUMBNAIL NOT CAPTURED")
                {
                    widget->SetPos(XMFLOAT2(current.x, statusY));
                }
                else if (name == "Import Model Commit")
                {
                    widget->SetPos(XMFLOAT2(current.x, confirmY));
                }
                else if (name == "Cancel Model Import")
                {
                    widget->SetPos(XMFLOAT2(current.x, cancelY));
                }
            }
        }

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

            auto& scene = session->Scenes().GetScene();
            sceneWeatherBefore_ = Capture(scene.weather);
            weatherEntity_ = session->Scenes().WeatherEntity();
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
                auto& scene = session->Scenes().GetScene();
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

        HiddenCallback hiddenCallback_;
        bool previewWeatherCaptured_ = false;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        WeatherPresentationState sceneWeatherBefore_;
        WeatherPresentationState entityWeatherBefore_;
    };
}
