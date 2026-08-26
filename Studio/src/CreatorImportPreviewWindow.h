#pragma once

#include <string>

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
        void SetVisible(const bool visible)
        {
            if (visible && !previewWeatherCaptured_)
            {
                CaptureAndNeutralizePreviewWeather();
            }
            else if (!visible && previewWeatherCaptured_)
            {
                RestorePreviewWeather();
            }

            wi::gui::Window::SetVisible(visible);
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

        bool previewWeatherCaptured_ = false;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        WeatherPresentationState sceneWeatherBefore_;
        WeatherPresentationState entityWeatherBefore_;
    };
}
