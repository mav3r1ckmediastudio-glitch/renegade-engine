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
            const float distance = 4400.0f + Random01() * 2400.0f;
            const XMFLOAT3 center{
                observer.x + distance * std::cos(theta),
                cloudStart + std::min(0.16f * cloudThickness, 750.0f),
                observer.z + distance * std::sin(theta)};
            const float side = theta + XM_PIDIV2;
            const float spread = settings_.clusterSpreadMetres;
            for (unsigned i = 0; i < frame_.cells.size(); ++i)
            {
                const float offset = (float(i) - 1.0f) * spread;
                const float randomShift = (Random01() - 0.5f) * spread * 0.45f;
                frame_.cells[i].position = {
                    center.x + std::cos(side) * (offset + randomShift),
                    center.y + (Random01() - 0.5f) * 0.12f * cloudThickness,
                    center.z + std::sin(side) * (offset + randomShift)};
                frame_.cells[i].power = 0.0f;
                frame_.cells[i].rangeMetres =
                    settings_.cloudLightRangeMetres * (0.74f + 0.28f * Random01());
            }
            ++frame_.eventId;
            frame_.flashing = true;
            eventAge_ = 0.0f;
        }

        SheetLightningSettings settings_{};
        SheetLightningFrame frame_{};
        std::uint32_t rng_ = 0x51EE7u;
        float nextSeconds_ = 0.0f;
        float eventAge_ = 1.0f;
    };
}
