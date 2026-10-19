#pragma once
#include "renegade/bridge/ScreenService.h"
#include <cmath>

namespace renegade::bridge
{
    // Instance presentation only: never mutates a project or a linked theme.
    struct ScreenAccessibilitySettings
    {
        float textScale=1;
        int contrast=0; // authored, white-on-black, black-on-white, yellow-on-black
        int focusStyle=0; // white, yellow, cyan (all with black outer stroke)
        float focusWidth=2;
        float uiScale=1;
        float minimumTargetSize=0; // logical output pixels; zero preserves authored targets
    };
    inline bool ValidateScreenAccessibilitySettings(const ScreenAccessibilitySettings& s) noexcept
    {
        return std::isfinite(s.textScale)&&s.textScale>=1&&s.textScale<=2&&
            s.contrast>=0&&s.contrast<=3&&s.focusStyle>=0&&s.focusStyle<=2&&
            std::isfinite(s.focusWidth)&&s.focusWidth>=1&&s.focusWidth<=6&&
            std::isfinite(s.uiScale)&&s.uiScale>=1&&s.uiScale<=2&&
            std::isfinite(s.minimumTargetSize)&&s.minimumTargetSize>=0&&s.minimumTargetSize<=96;
    }
    inline ScreenColor ScreenAccessibleFocusColor(const ScreenAccessibilitySettings& s) noexcept
    {
        if(s.focusStyle==1)return {255,255,0,255};
        if(s.focusStyle==2)return {0,255,255,255};
        return {255,255,255,255};
    }
    inline ScreenWidgetStyle ResolveAccessibleScreenStyle(const ScreenWidgetStyle& authored,
        ScreenWidgetKind kind,const ScreenAccessibilitySettings& settings)
    {
        auto style=authored;
        if(!ValidateScreenAccessibilitySettings(settings))return style;
        style.text.fontSize*=settings.textScale;
        style.text.characterSpacing*=settings.textScale;
        style.text.lineSpacing*=settings.textScale;
        // Enlargement requests actual line wrapping; it never scales the bitmap
        // or shrinks text back to fit. Authored rectangles/hit geometry stay exact.
        if(settings.textScale>1&&kind!=ScreenWidgetKind::TextInput)style.text.wrap=true;
        if(settings.contrast==0||kind==ScreenWidgetKind::Image)
            return style;
        const ScreenColor ink=settings.contrast==2?ScreenColor{0,0,0,255}:
            settings.contrast==3?ScreenColor{255,255,0,255}:ScreenColor{255,255,255,255};
        const ScreenColor paper=settings.contrast==2?ScreenColor{255,255,255,255}:ScreenColor{0,0,0,255};
        for(auto* state:{&style.normal,&style.hover,&style.pressed,&style.focused,&style.disabled})
        {
            state->background=paper;state->foreground=ink;
            // A texture cannot silently defeat the player's contrast selection.
            state->imageResourcePath.clear();
        }
        // Disabled controls retain a visible non-colour difference (no focus/input)
        // plus a contrasting outline. Focus remains an independent outline.
        style.borderColor=ink;style.borderWidth=std::max(1.0f,style.borderWidth);
        style.text.shadowColor={0,0,0,0};
        return style;
    }
}
