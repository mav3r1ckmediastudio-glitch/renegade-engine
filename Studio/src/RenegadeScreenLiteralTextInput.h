#pragma once
#include "RenegadeStudioChrome.h"
namespace renegade::studio
{
    // Screen labels and user text are literal. Numeric property inputs retain
    // the native arithmetic entry convenience when literal mode is disabled.
    class RenegadeScreenLiteralTextInput : public RenegadeTextInputField
    {
    public:
        void SetLiteral(bool value) noexcept{literal_=value;}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            if(literal_&&dt>0&&GetState()==wi::gui::ACTIVE&&
                wi::input::Press(wi::input::KEYBOARD_BUTTON_ENTER))
            {
                const auto raw=GetCurrentInputValue();
                SetValue(raw);font_input.SetText("");Deactivate();
                if(onInputAccepted){wi::gui::EventArgs args;args.sValue=raw;onInputAccepted(args);}
            }
            RenegadeTextInputField::Update(canvas,dt);
        }
    private:
        bool literal_=false;
    };
}
