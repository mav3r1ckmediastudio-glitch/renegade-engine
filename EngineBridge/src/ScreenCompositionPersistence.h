#pragma once
#include "renegade/bridge/ScreenComposition.h"
#include <wiConfig.h>
#include <optional>
#include <sstream>

namespace renegade::bridge::detail
{
    inline void WriteScreenComposition(wi::config::Section& section,
        const std::optional<ScreenCompositionPolicy>& policy)
    {
        section.Set("composition_override",policy.has_value());
        if(!policy)return;
        section.Set("ui_input",static_cast<int>(policy->uiInput));
        section.Set("cursor_policy",static_cast<int>(policy->cursor));
        section.Set("block_gameplay",policy->blockGameplayInput);
        section.Set("pause_gameplay",policy->pauseGameplay);
        section.Set("keyboard_focus",policy->takeKeyboardFocus);
        section.Set("composition_order",policy->order);
        section.Set("show_hud_underlay",policy->showHudUnderlay);
    }
    inline bool ReadScreenComposition(const wi::config::Section& section,
        std::optional<ScreenCompositionPolicy>& policy,std::string& error,ScreenCompositionLayer layer)
    {
        // Existing v3 documents predate authored overrides and retain usage defaults.
        if(!section.Has("composition_override")){policy.reset();return true;}
        const auto boolean=[&](const char* key,bool& value)
        {
            if(!section.Has(key))return false;
            const auto text=section.GetText(key);
            if(text!="true" && text!="false")return false;
            value=text=="true";return true;
        };
        bool overridden=false;
        if(!boolean("composition_override",overridden))
        {error="Screen composition override must be an explicit Boolean.";return false;}
        if(!overridden){policy.reset();return true;}
        const auto integer=[&](const char* key,int& value)
        {
            if(!section.Has(key))return false;
            std::istringstream text(section.GetText(key));text.imbue(std::locale::classic());
            text>>std::noskipws>>value;
            return !text.fail() && text.peek()==std::char_traits<char>::eof();
        };
        auto candidate=DefaultScreenCompositionPolicy(layer);int input=0,cursor=0;
        if(!integer("ui_input",input) || !integer("cursor_policy",cursor) ||
            !integer("composition_order",candidate.order) ||
            !boolean("block_gameplay",candidate.blockGameplayInput) ||
            !boolean("pause_gameplay",candidate.pauseGameplay) ||
            !boolean("keyboard_focus",candidate.takeKeyboardFocus))
        {error="Screen composition override has a missing or malformed field.";return false;}
        if(section.Has("show_hud_underlay") && !boolean("show_hud_underlay",candidate.showHudUnderlay))
        {error="Screen HUD underlay policy must be an explicit Boolean.";return false;}
        candidate.uiInput=static_cast<ScreenUiInputPolicy>(input);
        candidate.cursor=static_cast<ScreenCursorPolicy>(cursor);
        if(!ValidateScreenCompositionPolicy(candidate,error))return false;
        policy=candidate;return true;
    }
}
