#pragma once
#include <WickedEngine.h>
#include <array>

namespace renegade::studio
{
    class StudioColourWheel : public wi::gui::ColorPicker
    {
    public:
        void HideFields()
        {
            for (auto* field : std::array<wi::gui::Widget*, 8>{
                &text_R, &text_G, &text_B, &text_H, &text_S, &text_V, &text_hex, &alphaSlider})
                field->SetVisible(false);
        }
        void ResizeLayout() override
        {
            HideFields();
            wi::gui::ColorPicker::ResizeLayout();
        }
    };
}
