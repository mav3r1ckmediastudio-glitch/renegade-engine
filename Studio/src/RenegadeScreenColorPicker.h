#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include "StudioColourWheel.h"
#include "ScreenProjectPalette.h"

#include "renegade/bridge/ScreenService.h"

namespace renegade::studio
{
    class RenegadeScreenColorPicker final
    {
    public:
        void Open(
            const std::string& projectRoot, std::string title,
            const bridge::ScreenColor color,
            std::function<void(const bridge::ScreenColor&)> accepted)
        {
            title_ = std::move(title);
            color_ = color;
            accepted_ = std::move(accepted);
            if (!created_)
            {
                wheel_.Create("", wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
                wheel_.SetControlSize(0);
                wheel_.HideFields();
                wheel_.OnColorChanged([this](const wi::gui::EventArgs&) {
                    auto picked = wheel_.GetPickColor();
                    color_.red = picked.getR(); color_.green = picked.getG(); color_.blue = picked.getB();
                });
                created_ = true;
            }
            SetColour(color);
            palette_.Load(projectRoot);
            page_ = 0;
            open_ = true;
        }

        void Close() noexcept
        {
            open_ = false;
            draggingAlpha_ = false;
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

        const bridge::ScreenColor& Colour() const noexcept { return color_; }

        void Update(const wi::Canvas& canvas, float dt)
        {
            if (!open_) return;
            if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)) { Close(); return; }
            wheel_.SetPos({x_ + 8, y_ + 28});
            wheel_.SetSize({width_ - 16, height_ - 184});
            wheel_.Update(canvas, dt);
            const auto pointer = wi::input::GetPointer();
            const bool pressed = wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
            const float bottom = y_ + height_;
            auto inside = [&](float x, float y, float w, float h) {
                return pointer.x >= x && pointer.x < x+w && pointer.y >= y && pointer.y < y+h;
            };
            const float left = x_ + 92, track = width_ - 104;
            if (pressed && inside(left,bottom-152,track,24)) draggingAlpha_ = true;
            if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)) draggingAlpha_ = false;
            if (draggingAlpha_)
                color_.alpha = uint8_t(std::round(std::clamp((pointer.x-left)/track,0.f,1.f)*255));
            if (!pressed) return;
            if (inside(x_+width_-132,bottom-120,120,25))
            {
                palette_.Save(color_);
                if (!palette_.colours.empty()) page_ = (palette_.colours.size()-1)/Slots;
            }
            else if (inside(x_+8,bottom-88,24,28) && page_>0) --page_;
            else if (inside(x_+width_-32,bottom-88,24,28) && (page_+1)*Slots<palette_.colours.size()) ++page_;
            else if (inside(x_+36,bottom-88,width_-72,28))
            {
                const auto index = page_*Slots + size_t((pointer.x-x_-36)/((width_-72)/Slots));
                if (index < palette_.colours.size()) SetColour(palette_.colours[index]);
            }
            else if (inside(x_+10,bottom-38,(width_-30)/2,28))
            {
                auto callback = accepted_;
                auto result = color_;
                Close();
                if (callback) callback(result);
            }
            else if (inside(x_+20+(width_-30)/2,bottom-38,(width_-30)/2,28)) Close();
        }

        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const
        {
            if (!open_) return;
            const auto panel = wi::Color(33,37,40,255);
            const auto control = wi::Color(55,59,62,255);
            const auto white = wi::Color(244,244,244,255);
            DrawRect(x_, y_, width_, height_, panel, cmd);
            DrawLabel(title_,x_+12,y_+8,13,white,cmd);
            wheel_.Render(canvas,cmd);
            wi::graphics::Rect scissor{0,0,int(canvas.GetPhysicalWidth()),int(canvas.GetPhysicalHeight())};
            wi::graphics::GetDevice()->BindScissorRects(1,&scissor,cmd);
            const float bottom = y_+height_;
            auto button = [&](float x,float y,float w,const std::string& label) {
                DrawRect(x,y,w,25,control,cmd);
                DrawLabel(label,x+6,y+5,11,white,cmd);
            };
            DrawLabel("Opacity "+std::to_string(int(std::round(color_.alpha*100.f/255)))+"%",x_+10,bottom-147,11,white,cmd);
            DrawRect(x_+92,bottom-147,width_-104,13,control,cmd);
            DrawRect(x_+92,bottom-147,(width_-104)*color_.alpha/255.f,13,white,cmd);
            DrawLabel("Project palette",x_+10,bottom-115,12,white,cmd);
            button(x_+width_-132,bottom-120,120,"Save to palette");
            if (page_) button(x_+8,bottom-88,24,"<");
            if ((page_+1)*Slots<palette_.colours.size()) button(x_+width_-32,bottom-88,24,">");
            const float w = (width_-72)/Slots;
            for (size_t i=0;i<Slots && page_*Slots+i<palette_.colours.size();++i)
            {
                const auto& c=palette_.colours[page_*Slots+i];
                const float x=x_+36+i*w;
                DrawRect(x,bottom-88,w-3,25,wi::Color(170,170,170,255),cmd);
                DrawRect(x,bottom-88,(w-3)*.5f,25,wi::Color(80,80,80,255),cmd);
                DrawRect(x,bottom-88,w-3,25,wi::Color(c.red,c.green,c.blue,c.alpha),cmd);
            }
            DrawLabel(palette_.status,x_+10,bottom-57,10,white,cmd);
            const float half=(width_-30)/2;
            button(x_+10,bottom-38,half,"Apply");
            button(x_+20+half,bottom-38,half,"Cancel");
        }

    private:
        static constexpr size_t Slots = 8;
        StudioColourWheel wheel_;
        ScreenProjectPalette palette_;
        size_t page_ = 0;
        bool created_ = false;
        bool draggingAlpha_ = false;
        void SetColour(bridge::ScreenColor c)
        {
            wheel_.SetPickColor(wi::Color(c.red,c.green,c.blue,c.alpha));
            color_ = c;
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
