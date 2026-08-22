#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "renegade/bridge/ScreenService.h"

namespace renegade::studio
{
    class RenegadeScreenColorPicker final
    {
    public:
        void Open(
            std::string title,
            const bridge::ScreenColor color,
            std::function<void(const bridge::ScreenColor&)> accepted)
        {
            title_ = std::move(title);
            color_ = color;
            accepted_ = std::move(accepted);
            open_ = true;
        }

        void Close() noexcept
        {
            open_ = false;
            accepted_ = {};
        }

        [[nodiscard]] bool IsOpen() const noexcept
        {
            return open_;
        }

        void SetLayout(
            const float x,
            const float y,
            const float width,
            const float height) noexcept
        {
            x_ = x;
            y_ = y;
            width_ = std::max(260.0f, width);
            height_ = std::max(260.0f, height);
        }

        void Update()
        {
            if (!open_) return;
            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool pressed = wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
            const bool down = wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);

            for (int channel = 0; channel < 4; ++channel)
            {
                const float rowY = y_ + 92.0f + static_cast<float>(channel) * 45.0f;
                const float barX = x_ + 72.0f;
                const float barW = width_ - 92.0f;
                if ((pressed || down) && pointer.x >= barX &&
                    pointer.x <= barX + barW && pointer.y >= rowY &&
                    pointer.y <= rowY + 22.0f)
                {
                    const float normalized = std::clamp(
                        (pointer.x - barX) / barW, 0.0f, 1.0f);
                    SetChannel(channel, static_cast<std::uint8_t>(
                        std::round(normalized * 255.0f)));
                }
            }

            if (!pressed) return;
            const float buttonY = y_ + height_ - 42.0f;
            const float half = (width_ - 30.0f) * 0.5f;
            if (pointer.y >= buttonY && pointer.y <= buttonY + 30.0f)
            {
                if (pointer.x >= x_ + 10.0f && pointer.x <= x_ + 10.0f + half)
                {
                    auto callback = accepted_;
                    const auto acceptedColor = color_;
                    Close();
                    if (callback) callback(acceptedColor);
                }
                else if (pointer.x >= x_ + 20.0f + half &&
                         pointer.x <= x_ + width_ - 10.0f)
                {
                    Close();
                }
            }
        }

        void Render(const wi::graphics::CommandList cmd) const
        {
            if (!open_) return;
            DrawRect(x_, y_, width_, height_, wi::Color(4, 8, 12, 252), cmd);
            DrawBorder(x_, y_, width_, height_, wi::Color(39, 183, 222, 255), cmd);
            DrawLabel(title_, x_ + 12.0f, y_ + 10.0f, 11,
                wi::Color(244, 244, 244, 255), cmd);

            DrawRect(x_ + 12.0f, y_ + 38.0f, width_ - 24.0f, 34.0f,
                wi::Color(color_.red, color_.green, color_.blue, color_.alpha), cmd);
            DrawBorder(x_ + 12.0f, y_ + 38.0f, width_ - 24.0f, 34.0f,
                wi::Color(120, 140, 150, 255), cmd);

            const char* labels[] = {"R", "G", "B", "A"};
            const std::uint8_t values[] = {
                color_.red, color_.green, color_.blue, color_.alpha};
            for (int channel = 0; channel < 4; ++channel)
            {
                const float rowY = y_ + 92.0f + static_cast<float>(channel) * 45.0f;
                const float barX = x_ + 72.0f;
                const float barW = width_ - 92.0f;
                DrawLabel(std::string(labels[channel]) + "  " +
                    std::to_string(values[channel]), x_ + 12.0f, rowY + 4.0f,
                    10, wi::Color(220, 230, 235, 255), cmd);
                DrawRect(barX, rowY, barW, 22.0f,
                    wi::Color(28, 38, 44, 255), cmd);
                DrawRect(barX, rowY,
                    barW * (static_cast<float>(values[channel]) / 255.0f),
                    22.0f, ChannelColor(channel), cmd);
                DrawBorder(barX, rowY, barW, 22.0f,
                    wi::Color(70, 90, 100, 255), cmd);
            }

            const float buttonY = y_ + height_ - 42.0f;
            const float half = (width_ - 30.0f) * 0.5f;
            DrawRect(x_ + 10.0f, buttonY, half, 30.0f,
                wi::Color(13, 55, 68, 255), cmd);
            DrawRect(x_ + 20.0f + half, buttonY, half, 30.0f,
                wi::Color(45, 28, 30, 255), cmd);
            DrawLabel("APPLY", x_ + 24.0f, buttonY + 8.0f, 9,
                wi::Color(244, 244, 244, 255), cmd);
            DrawLabel("CANCEL", x_ + 34.0f + half, buttonY + 8.0f, 9,
                wi::Color(244, 244, 244, 255), cmd);
        }

    private:
        void SetChannel(const int channel, const std::uint8_t value) noexcept
        {
            if (channel == 0) color_.red = value;
            else if (channel == 1) color_.green = value;
            else if (channel == 2) color_.blue = value;
            else color_.alpha = value;
        }

        [[nodiscard]] static wi::Color ChannelColor(const int channel) noexcept
        {
            if (channel == 0) return wi::Color(220, 70, 70, 255);
            if (channel == 1) return wi::Color(70, 220, 100, 255);
            if (channel == 2) return wi::Color(70, 120, 235, 255);
            return wi::Color(210, 210, 210, 255);
        }

        static void DrawRect(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            wi::image::Params params(x, y, w, h, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawBorder(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, w, 1.0f, color, cmd);
            DrawRect(x, y + h - 1.0f, w, 1.0f, color, cmd);
            DrawRect(x, y, 1.0f, h, color, cmd);
            DrawRect(x + w - 1.0f, y, 1.0f, h, color, cmd);
        }

        static void DrawLabel(
            const std::string& value,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            wi::font::Params params(x, y, size,
                wi::font::WIFALIGN_LEFT, wi::font::WIFALIGN_TOP,
                color, wi::Color::Transparent());
            params.bolden = 0.1f;
            wi::font::Draw(value, params, cmd);
        }

        std::string title_;
        bridge::ScreenColor color_;
        std::function<void(const bridge::ScreenColor&)> accepted_;
        bool open_ = false;
        float x_ = 0.0f;
        float y_ = 0.0f;
        float width_ = 320.0f;
        float height_ = 320.0f;
    };
}
