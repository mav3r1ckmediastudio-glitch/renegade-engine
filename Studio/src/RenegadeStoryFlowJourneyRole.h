#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace renegade::studio
{
    enum class JourneyBranchRole
    {
        Main,
        Options,
        LoadSave,
        Failure,
        Detached,
        Custom,
    };

    // One classification seam drives both branch-lane accents and Inspector
    // exit bullets, so colour can never disagree between those two surfaces.
    [[nodiscard]] inline JourneyBranchRole JourneyRoleForOutcome(
        std::string outcome,
        const bool detached = false)
    {
        if (detached)
            return JourneyBranchRole::Detached;
        std::transform(outcome.begin(), outcome.end(), outcome.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        const auto contains = [&](const char* token)
        {
            return outcome.find(token) != std::string::npos;
        };
        if (outcome == "next" || contains("new_game") ||
            contains("flow.start"))
        {
            return JourneyBranchRole::Main;
        }
        if (contains("option") || contains("setting") ||
            contains("audio") || contains("video") || contains("control"))
        {
            return JourneyBranchRole::Options;
        }
        if (contains("load") || contains("save") ||
            contains("slot") || contains("continue"))
        {
            return JourneyBranchRole::LoadSave;
        }
        if (contains("death") || contains("fail") || contains("respawn") ||
            contains("restart") || contains("quit") || contains("lose"))
        {
            return JourneyBranchRole::Failure;
        }
        return JourneyBranchRole::Custom;
    }
}
