#pragma once

// Weather V2 G4-S1. Invisible in-cloud flashes, NOT visible forked lightning.
// The visual is created by native cloud-scattering enabled point lights.
// This file contains a deterministic, render-independent event scheduler.

#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace renegade::bridge
{
    struct SheetLightningSettings
    {
        bool enabled = false;
        float eventsPerMinute = 8.0f;
        float intensity = 1.0f;
        float clusterSpreadMetres = 340.0f;
        float cloudLightRangeMetres = 1050.0f;
        std::uint32_t seed = 0x51EE7u;
    };

    struct SheetLightningCell
    {
        XMFLOAT3 position{};
        float power = 0.0f; // normalized pulse amplitude; no geometry
        float rangeMetres = 900.0f;
    };

    struct SheetLightningFrame
    {
        std::array<SheetLightningCell, 3> cells{};
        std::uint64_t eventId = 0;
        bool flashing = false;
        // Reprojection stays responsive briefly after the light is gone,
        // preventing old bright checkerboard history from lingering.
        bool temporalRecovery = false;
    };

    class SheetLightningController
    {
    public:
        void Configure(const SheetLightningSettings& requested) noexcept
        {
            settings_ = requested;
            auto safe = [](float v, float fallback, float low, float high)
            {
                return std::clamp(std::isfinite(v) ? v : fallback, low, high);
            };
            settings_.eventsPerMinute =
                safe(requested.eventsPerMinute, 8.0f, 0.0f, 30.0f);
            settings_.intensity =
                safe(requested.intensity, 1.0f, 0.0f, 3.0f);
            settings_.clusterSpreadMetres =
                safe(requested.clusterSpreadMetres, 340.0f, 80.0f, 850.0f);
            settings_.cloudLightRangeMetres =
                safe(requested.cloudLightRangeMetres, 1050.0f, 200.0f, 2400.0f);
            Reset();
        }

        void Reset() noexcept
        {
            frame_ = {};
            eventAge_ = 1.0f;
            rng_ = settings_.seed ? settings_.seed : 0x51EE7u;
            frame_.eventId = 0;
            // Quick first occurrence for opt-in visual review; ordinary
            // authoring has the feature disabled until explicitly enabled.
            nextSeconds_ = settings_.enabled && settings_.eventsPerMinute > 0.0f
                ? 1.5f + Random01() * 2.5f : 0.0f;
        }

        void Advance(float dt,
                     const XMFLOAT3& observer,
                     float cloudStartMetres,
                     float cloudThicknessMetres,
                     const XMFLOAT3& viewDirection = XMFLOAT3(0.0f, 0.0f, 1.0f)) noexcept
        {
            if (!settings_.enabled ||
                settings_.eventsPerMinute <= 0.0f ||
                !std::isfinite(dt) || dt <= 0.0f)
                return;
            // No large hitches creating many catch-up flashes.
            dt = std::min(dt, 0.25f);
            eventAge_ += dt;
            const float age = eventAge_;
            frame_.flashing = age >= 0.0f && age < 0.78f;
            frame_.temporalRecovery = frame_.eventId > 0 &&
                age >= 0.0f && age < 1.12f;
            // Two uneven return-pulses and a weak final internal shimmer;
            // spatial staggering makes a luminous region travel inside the
            // existing true volume, without bolt geometry or a screen overlay.
            const float first = Pulse(age, 0.000f, 0.16f);
            const float second = Pulse(age, 0.21f, 0.26f) * 0.85f;
            const float third = Pulse(age, 0.51f, 0.21f) * 0.50f;
            const float amplitudes[3] = {
                first * 0.95f + second * 0.25f,
                first * 0.30f + second * 0.9f + third * 0.40f,
                second * 0.20f + third * 0.98f
            };
            for (unsigned i = 0; i < frame_.cells.size(); ++i)
                frame_.cells[i].power = std::min(1.0f, amplitudes[i]) *
                    settings_.intensity;
            if (age > 0.78f) frame_.flashing = false;

            nextSeconds_ -= dt;
            if (nextSeconds_ > 0.0f) return;
            StartEvent(observer, cloudStartMetres, cloudThicknessMetres, viewDirection);
            const float base = 60.0f / settings_.eventsPerMinute;
            nextSeconds_ = base * (0.7f + 0.7f * Random01());
        }

        // G4 fork lightning can use the *same approved* three-cell
        // formation, pulse and fading, but anchored to its real strike
        // origin. The ordinary autonomous sheet scheduler is unchanged.
        void TriggerAtPosition(const XMFLOAT3& cloudOrigin,
                               float cloudThicknessMetres) noexcept
        {
            if (!settings_.enabled ||
                !std::isfinite(cloudOrigin.x) ||
                !std::isfinite(cloudOrigin.y) ||
                !std::isfinite(cloudOrigin.z))
                return;
            // StartEvent constructs the existing approved cells including
            // per-event radius variation and softness. Translate the group
            // rigidly instead of inventing new circular lighting logic.
            StartEvent(cloudOrigin,
                cloudOrigin.y, cloudThicknessMetres, XMFLOAT3(1, 0, 0));
            XMFLOAT3 centroid{};
            for (const auto& cell : frame_.cells)
            {
                centroid.x += cell.position.x / 3.0f;
                centroid.y += cell.position.y / 3.0f;
                centroid.z += cell.position.z / 3.0f;
            }
            for (auto& cell : frame_.cells)
            {
                cell.position.x += cloudOrigin.x - centroid.x;
                cell.position.y += cloudOrigin.y - centroid.y;
                cell.position.z += cloudOrigin.z - centroid.z;
            }
            // Fork events, not the independent sheet clock, schedule this
            // particular controller. The fork-driven renderer uses
            // FrameDrivenByStrike rather than advancing this pulse timer.
            nextSeconds_ = 3600.0f;
        }

        // A visible fork supplies its own return-stroke envelope. Reuse
        // the approved sheet formation and pulse intensity, but NEVER run
        // the independent sheet controller's second/third pulse schedule.
        // This additive API leaves normal sheet lightning unchanged.
        [[nodiscard]] SheetLightningFrame FrameDrivenByStrike(
            float forkBrightness, float strikeAgeSeconds) const noexcept
        {
            if (frame_.eventId == 0 || !std::isfinite(strikeAgeSeconds) ||
                strikeAgeSeconds < 0.0f)
                return {};
            SheetLightningFrame driven = frame_;
            const float amplitude = std::clamp(
                std::isfinite(forkBrightness) ? forkBrightness : 0.0f,
                0.0f, 1.0f) * settings_.intensity;
            // Same approved 3-cell volume geometry and shader, with
            // unequal cell energies for spatial diffusion, not a screen flash.
            constexpr float cellWeights[3] = {0.95f, 0.50f, 0.30f};
            for (unsigned i = 0; i < driven.cells.size(); ++i)
                driven.cells[i].power = amplitude * cellWeights[i];
            driven.flashing = amplitude > 0.001f;
            // Temporal cleanup briefly outlives the *last* return stroke
            // but carries no new radiance once the visible bolt is gone.
            driven.temporalRecovery = strikeAgeSeconds < 0.62f;
            return driven;
        }

        [[nodiscard]] const SheetLightningFrame& Frame() const noexcept
        {
            return frame_;
        }
        [[nodiscard]] float TimeUntilNextEvent() const noexcept
        {
            return nextSeconds_;
        }

    private:
        static float Pulse(float age, float start, float duration) noexcept
        {
            if (age < start || age >= start + duration) return 0.0f;
            const float t = (age - start) / duration;
            // Fast rise + curved fade. Different per-cell staggered
            // weights create soft, localized volumetric illumination.
            return std::min(1.0f, t * 10.0f) * (1.0f - t) * (1.0f - t);
        }

        std::uint32_t Random() noexcept
        {
            rng_ ^= rng_ << 13u;
            rng_ ^= rng_ >> 17u;
            rng_ ^= rng_ << 5u;
            return rng_;
        }

        float Random01() noexcept
        {
            return float(Random() & 0x00ffffffu) / 16777216.0f;
        }

        void StartEvent(const XMFLOAT3& observer,
                        float cloudStart, float cloudThickness,
                        const XMFLOAT3& viewDirection) noexcept
        {
            cloudStart = std::isfinite(cloudStart)
                ? std::max(100.0f, cloudStart) : 1200.0f;
            cloudThickness = std::isfinite(cloudThickness)
                ? std::clamp(cloudThickness, 200.0f, 10000.0f) : 3500.0f;
            // Reviewer-facing spatial placement follows the camera's
            // horizontal view cone, rather than flashing invisibly behind
            // the player. All offsets remain cloud-world-space and do not
            // behave like a screen-space overlay.
            const float heading = std::hypot(viewDirection.x, viewDirection.z) > 0.01f
                ? std::atan2(viewDirection.z, viewDirection.x) : 0.0f;
            const float theta = heading + (Random01() - 0.5f) * 1.25f;
            const float distance = 7800.0f + Random01() * 2800.0f;
            const XMFLOAT3 center{
                observer.x + distance * std::cos(theta),
                cloudStart + std::min(0.095f * cloudThickness, 440.0f),
                observer.z + distance * std::sin(theta)};
            const float side = theta + XM_PIDIV2;
            const float spread = settings_.clusterSpreadMetres;
            for (unsigned i = 0; i < frame_.cells.size(); ++i)
            {
                const float offset = (float(i) - 1.0f) * spread;
                const float randomShift = (Random01() - 0.5f) * spread * 0.45f;
                frame_.cells[i].position = {
                    center.x + std::cos(side) * (offset + randomShift),
                    center.y + (Random01() - 0.5f) * 0.09f * cloudThickness,
                    center.z + std::sin(side) * (offset + randomShift)};
                frame_.cells[i].power = 0.0f;
                frame_.cells[i].rangeMetres =
                    settings_.cloudLightRangeMetres * (0.74f + 0.28f * Random01());
            }
            ++frame_.eventId;
            frame_.flashing = true;
            frame_.temporalRecovery = true;
            eventAge_ = 0.0f;
        }

        SheetLightningSettings settings_{};
        SheetLightningFrame frame_{};
        std::uint32_t rng_ = 0x51EE7u;
        float nextSeconds_ = 0.0f;
        float eventAge_ = 1.0f;
    };
}
