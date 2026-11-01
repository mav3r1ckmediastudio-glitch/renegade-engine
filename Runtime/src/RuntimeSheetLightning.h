#pragma once
// G4-S1: preview only. Inject sheet lightning radiance directly into the
// existing cloud march. No mesh bolts, persistent lights or saved level data.
#include "renegade/bridge/SheetLightningService.h"
#include "RuntimeThunderAudio.h"
#include "renegade/bridge/LightningAuthoringService.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

namespace renegade::runtime
{
    class RuntimeSheetLightning
    {
    public:
        void Update(wi::scene::Scene& scene,
                    std::uint64_t sceneRevision,
                    const XMFLOAT3& observer,
                    const XMFLOAT3& viewDirection,
                    float gameDelta)
        {
            bridge::LightningAuthoringState authored;
            if(scene.weathers.GetCount())
                authored=bridge::CaptureLightning(scene,scene.weathers.GetEntity(0));
            const bool active=authored.mode==bridge::LightningMode::Sheet ||
                              authored.mode==bridge::LightningMode::Both;
            const bool preview=PreviewRequested();
            if(!active && !preview) {
                if(initialized_)Reset(scene);
                return;
            }
            const float frequency=preview && !active?10.0f:authored.sheetPerMinute;
            const float intensity=preview && !active?.14f:authored.sheetIntensity;

            if (!initialized_ || sceneRevision_ != sceneRevision ||
                frequency_!=frequency || intensity_!=intensity)
            {
                bridge::SheetLightningSettings opts;
                opts.enabled = true;
                opts.eventsPerMinute = frequency;
                opts.intensity = intensity; // Wider lobes overlap more; keep core from blooming.
                opts.clusterSpreadMetres = 720.0f;
                opts.cloudLightRangeMetres = 2340.0f; // G4: ~30% wider per-cell reach.
                schedule_.Configure(opts);
                frequency_ = frequency; intensity_ = intensity;
                sceneRevision_ = sceneRevision;
                initialized_ = true;
                lastEvent_ = 0;
                tracedPeak_ = false;
                Trace("INIT sheet lightning CLOUD-SCALE SHADER path");
            }

            if (!scene.weather.IsVolumetricClouds())
            {
                SetCells(scene, {});
                return;
            }

            schedule_.Advance(gameDelta, observer,
                              scene.weather.volumetricCloudParameters.cloudStartHeight,
                              scene.weather.volumetricCloudParameters.cloudThickness,
                              viewDirection);

            const auto& flash = schedule_.Frame();
            SetCells(scene, flash);
            if (flash.eventId > 0 && flash.eventId != lastEvent_)
            {
                lastEvent_ = flash.eventId;
                tracedPeak_ = false;
                ThunderAudio().Queue(flash.cells[0].position,observer,authored.thunder && active || preview,authored.thunderVolume,authored.distanceDelay,authored.thunderSoundSet);
                Trace("FLASH_START id=" + std::to_string(lastEvent_) +
                    " cloudStart=" + std::to_string(
                        scene.weather.volumetricCloudParameters.cloudStartHeight) +
                    " cellX=" + std::to_string(flash.cells[0].position.x) +
                    " cellY=" + std::to_string(flash.cells[0].position.y) +
                    " range=" + std::to_string(flash.cells[0].rangeMetres));
            }
            if (!tracedPeak_ &&
                std::max({flash.cells[0].power, flash.cells[1].power,
                          flash.cells[2].power}) > 0.07f)
            {
                tracedPeak_ = true;
                Trace("FLASH_ACTIVE power=" +
                    std::to_string(flash.cells[0].power) + "," +
                    std::to_string(flash.cells[1].power) + "," +
                    std::to_string(flash.cells[2].power));
            }
        }

        void Reset(wi::scene::Scene& scene) noexcept
        {
            SetCells(scene, {});
            schedule_.Reset();
            initialized_ = false;
        }

        // Shared approved cloud-scattering payload for fork-strike origins.
        static void ApplyFrame(wi::scene::Scene& scene,
                               const bridge::SheetLightningFrame& flash) noexcept
        {
            SetCells(scene, flash);
        }

    private:
        static void SetCells(wi::scene::Scene& scene,
                             const bridge::SheetLightningFrame& flash) noexcept
        {
            auto toCell = [](const bridge::SheetLightningCell& c) noexcept
            {
                return XMFLOAT4(c.position.x, c.position.y, c.position.z,
                                c.rangeMetres);
            };
            const XMFLOAT4 cell0 = toCell(flash.cells[0]);
            const XMFLOAT4 cell1 = toCell(flash.cells[1]);
            const XMFLOAT4 cell2 = toCell(flash.cells[2]);
            const XMFLOAT4 power(flash.cells[0].power, flash.cells[1].power,
                                 flash.cells[2].power,
                                 flash.temporalRecovery ? 1.0f : 0.0f);
            auto apply = [&](wi::scene::WeatherComponent& w) noexcept
            {
                w.sheet_light_cell0 = cell0;
                w.sheet_light_cell1 = cell1;
                w.sheet_light_cell2 = cell2;
                w.sheet_light_power = power;
            };
            // The native scene update reselects the first WeatherComponent;
            // update both carrier and active weather so that it cannot
            // erase the transient radiance before GPU scene constants upload.
            apply(scene.weather);
            if (scene.weathers.GetCount() > 0)
                apply(scene.weathers[0]);
        }

        static bool PreviewRequested() noexcept
        {
            static const bool enabled = []()
            {
                const char* value = std::getenv(
                    "RENEGADE_G4_SHEET_LIGHTNING_PREVIEW");
                return value && value[0] == '1' && value[1] == '\0';
            }();
            return enabled;
        }

        static void Trace(const std::string& msg)
        {
            if (const char* path = std::getenv(
                    "RENEGADE_G4_SHEET_LIGHTNING_TRACE"))
            {
                std::ofstream file(path, std::ios::app);
                if (file) file << msg << '\n';
            }
        }

        bridge::SheetLightningController schedule_;
        std::uint64_t sceneRevision_ = 0;
        std::uint64_t lastEvent_ = 0;
        float frequency_=-1.0f, intensity_=-1.0f;
        bool initialized_ = false;
        bool tracedPeak_ = false;
    };
}
