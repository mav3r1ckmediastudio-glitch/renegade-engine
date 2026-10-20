#pragma once
#include <string>
#include <utility>
namespace renegade::studio
{
    // Template defaults stop owning the draft as soon as the user enters or types in the title.
    struct StoryFlowTitleDraft
    {
        std::string text;
        bool customized = false;
        void Input(std::string value) { text = std::move(value); customized = true; }
        bool UseTemplate(std::string value)
        {
            if (customized) return false;
            text = std::move(value);
            return true;
        }
    };
}
