#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

namespace renegade::studio
{
    class RenegadeScreenChoicePicker final
    {
    public:
        struct Choice
        {
            std::string label;
            std::string value;
        };

        void Open(
            std::string title,
            std::vector<Choice> choices,
            std::string currentValue,
            std::function<void(const std::string&)> selected)
        {
            title_ = std::move(title);
            choices_ = std::move(choices);
            currentValue_ = std::move(currentValue);
            selected_ = std::move(selected);
            scroll_ = 0;
            open_ = true;
        }

        void Close() noexcept
        {
            open_ = false;
            choices_.clear();
            currentValue_.clear();
            selected_ = {};
            scroll_ = 0;
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
            width_ = std::max(220.0f, width);
            height_ = std::max(160.0f, height);
        }

        void Update()
        {
            if (!open_) return;
            const XMFLOAT4 pointer = wi::input::GetPointer();
            if (!Contains(pointer.x, pointer.y)) return;

            const std::size_t visible = VisibleRows();
            if (pointer.z != 0.0f && choices_.size() > visible)
            {
                const std::size_t maximum = choices_.size() - visible;
                if (pointer.z < 0.0f)
                    scroll_ = std::min(scroll_ + 1, maximum);
                else if (scroll_ > 0)
                    --scroll_;
            }

            if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) return;
            const float rowsTop = y_ + HeaderHeight;
            if (pointer.y < rowsTop)
            {
                Close();
                return;
            }
            const std::size_t row = static_cast<std::size_t>(
                std::floor((pointer.y - rowsTop) / RowHeight));
            if (row >= visible) return;
            const std::size_t index = scroll_ + row;
            if (index >= choices_.size()) return;
            const std::string value = choices_[index].value;
            auto callback = selected_;
            Close();
            if (callback) callback(value);
        }

        void Render(const wi::graphics::CommandList cmd) const
        {
            if (!open_) return;
            DrawRect(x_, y_, width_, height_, wi::Color(4, 8, 12, 252), cmd);
            DrawBorder(x_, y_, width_, height_, wi::Color(39, 183, 222, 255), cmd);
            DrawLabel(title_, x_ + 12.0f, y_ + 10.0f, 11,
                wi::Color(244, 244, 244, 255), cmd);
            DrawLabel("CLICK HEADER TO CANCEL // MOUSE WHEEL TO SCROLL",
                x_ + 12.0f, y_ + 27.0f, 7,
                wi::Color(135, 151, 159, 255), cmd);

            const std::size_t visible = VisibleRows();
            const float rowsTop = y_ + HeaderHeight;
            for (std::size_t row = 0; row < visible; ++row)
            {
                const std::size_t index = scroll_ + row;
                if (index >= choices_.size()) break;
                const auto& choice = choices_[index];
                const float rowY = rowsTop + static_cast<float>(row) * RowHeight;
                if (choice.value == currentValue_)
                {
                    DrawRect(x_ + 7.0f, rowY + 2.0f, width_ - 14.0f,
                        RowHeight - 4.0f, wi::Color(13, 35, 45, 255), cmd);
                }
                DrawLabel(Short(choice.label, 46), x_ + 12.0f, rowY + 7.0f,
                    10, choice.value == currentValue_
                        ? wi::Color(244, 244, 244, 255)
                        : wi::Color(170, 185, 193, 255), cmd);
            }

            if (choices_.empty())
            {
                DrawLabel("NO GOVERNED CHOICES AVAILABLE",
                    x_ + 12.0f, rowsTop + 10.0f, 10,
                    wi::Color(222, 91, 29, 255), cmd);
            }
        }

    private:
        static constexpr float HeaderHeight = 48.0f;
        static constexpr float RowHeight = 30.0f;

        [[nodiscard]] bool Contains(const float px, const float py) const noexcept
        {
            return px >= x_ && py >= y_ &&
                px < x_ + width_ && py < y_ + height_;
        }

        [[nodiscard]] std::size_t VisibleRows() const noexcept
        {
            return static_cast<std::size_t>(std::max(
                1.0f, std::floor((height_ - HeaderHeight) / RowHeight)));
        }

        static std::string Short(const std::string& value, const std::size_t limit)
        {
            if (value.size() <= limit) return value;
            if (limit <= 3) return value.substr(0, limit);
            return value.substr(0, limit - 3) + "...";
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
        std::vector<Choice> choices_;
        std::string currentValue_;
        std::function<void(const std::string&)> selected_;
        std::size_t scroll_ = 0;
        bool open_ = false;
        float x_ = 0.0f;
        float y_ = 0.0f;
        float width_ = 320.0f;
        float height_ = 360.0f;
    };
}
