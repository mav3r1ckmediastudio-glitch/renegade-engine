#pragma once
#include "StudioTheme.h"
#include "StudioUiPolicy.h"
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace renegade::studio
{
    // The pinned native Button visits DEACTIVATING during a sustained hold.
    // Keep its focus/drag/priority handling, but expose action callbacks only
    // after release, as already required by the accepted dialogue editor.
    class ReleaseButton : public wi::gui::Button
    {
    public:
        void Update(const wi::Canvas& canvas, float dt) override
        {
            if (!IsEnabled() || dt <= 0 || wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
            {
                Widget::Update(canvas, dt);
                state = wi::gui::IDLE;
                return;
            }
            if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
            {
                wi::gui::Button::Update(canvas, dt);
                return;
            }
            auto callback = std::move(onClick);
            onClick = [](const wi::gui::EventArgs&) {};
            wi::gui::Button::Update(canvas, dt);
            onClick = std::move(callback);
        }
    };
}

namespace renegade::studio::theme
{
    inline void ButtonRect(float x, float y, float w, float h, wi::Color colour,
                           wi::graphics::CommandList cmd, float radius = 4.0f)
    {
        if (w <= 0 || h <= 0) return;
        wi::image::Params p(x, y, w, h, colour);
        if (radius > 0)
        {
            p.enableCornerRounding();
            for (auto& corner : p.corners_rounding) { corner.radius = radius; corner.segments = 6; }
        }
        wi::image::Draw(nullptr, p, cmd);
    }

    inline void DrawButton(float x, float y, float width, float height, std::string text,
                           const ui::ButtonVisual& state, int textSize,
                           wi::graphics::CommandList cmd, bool leftAligned = false,
                           bool swatch = false, wi::Color swatchColour = Idle)
    {
        const bool enabled = state.enabled;
        const float down = state.Depression();
        const auto edge = enabled && (state.hovered || state.pressed || state.primary || state.selected)
            ? Accent : Border;
        const auto face = !enabled ? Disabled : state.pressed ? Active :
            swatch ? swatchColour : state.hovered ? (state.primary ? AccentHover : Focus) :
            state.primary ? Accent : state.selected ? Selected : Control;
        ButtonRect(x, y, width, height, edge, cmd);
        ButtonRect(x + 1, y + 1 + down, width - 2, height - 2 - down, face, cmd, 3);

        // Disclosure arrows use geometry, not an optional font glyph.
        const bool closed = text.rfind("▶", 0) == 0;
        const bool open = text.rfind("▼", 0) == 0;
        if (closed || open)
        {
            text.erase(0, 3);
            while (!text.empty() && text.front() == ' ') text.erase(0, 1);
            const float cx = x + 13, cy = y + height * .5f + down;
            auto line = [&](float ax, float ay, float bx, float by)
            {
                const float dx = bx-ax, dy = by-ay;
                wi::image::Params p;
                p.pos = { (ax+bx)*.5f, (ay+by)*.5f, 0 };
                p.siz = { std::sqrt(dx*dx+dy*dy), 1.5f };
                p.pivot = { .5f, .5f }; p.rotation = std::atan2(dy,dx);
                p.color = enabled ? Text : DisabledText;
                wi::image::Draw(nullptr,p,cmd);
            };
            if (open) { line(cx-3,cy-2,cx,cy+2); line(cx,cy+2,cx+3,cy-2); }
            else { line(cx-2,cy-3,cx+2,cy); line(cx+2,cy,cx-2,cy+3); }
        }
        wi::font::Params metrics(0,0,textSize);
        metrics.bolden=.12f;
        const float available=std::max(1.f,width-(leftAligned?24.f:closed||open?46.f:20.f));
        if(wi::font::TextWidth(text,metrics)>available)
        {
            while(!text.empty() && wi::font::TextWidth(text+"...",metrics)>available)
            {
                size_t cut=text.size()-1;
                while(cut>0 && (static_cast<unsigned char>(text[cut])&0xc0)==0x80)--cut;
                text.resize(cut);
            }
            text+="...";
        }
        wi::font::Params fp(x + (leftAligned ? 12.f : width * .5f),
                            y + height * .5f + down, textSize,
                            leftAligned ? wi::font::WIFALIGN_LEFT : wi::font::WIFALIGN_CENTER,
                            wi::font::WIFALIGN_CENTER, enabled ? Text : DisabledText);
        fp.bolden = .12f;
        wi::font::Draw(text, fp, cmd);
    }
}
