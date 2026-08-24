#include "RenegadeStoryFlowJourneyLayout.h"
#include "RenegadeStoryFlowJourneyRole.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Near(const float value, const float expected)
    {
        return std::abs(value - expected) < 0.01f;
    }

    int Fail(const char* message)
    {
        std::cerr << "RenegadeStoryFlowJourneyUiLayoutTests: "
                  << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade::studio;

    const JourneyShellLayout concept =
        ComputeJourneyShellLayout(1680.0f, 945.0f);
    if (!Near(concept.topBar.height, 70.0f) ||
        !Near(concept.navigationRail.width, 96.0f) ||
        concept.journeyCanvas.width < 1260.0f ||
        concept.inspector.width < 305.0f ||
        concept.inspector.width > 315.0f)
    {
        return Fail("1680x945 concept geometry drifted");
    }

    const JourneyShellLayout fullHd =
        ComputeJourneyShellLayout(1920.0f, 1080.0f);
    if (!Near(fullHd.inspector.width, 336.0f) ||
        fullHd.storyOverview.Right() > fullHd.journeyCanvas.Right() ||
        fullHd.storyOverview.Bottom() > 1080.0f)
    {
        return Fail("1920x1080 fixed chrome escaped its host");
    }

    const JourneyShellLayout compact =
        ComputeJourneyShellLayout(1280.0f, 720.0f);
    if (!Near(compact.inspector.width, 280.0f) ||
        !Near(compact.journeyCanvas.width, 904.0f) ||
        compact.canvasNavigation.Bottom() > 720.0f ||
        compact.storyOverview.Bottom() > 720.0f)
    {
        return Fail("1280x720 responsive geometry drifted");
    }

    if (JourneyRoleForOutcome("options") != JourneyBranchRole::Options ||
        JourneyRoleForOutcome("load_game") != JourneyBranchRole::LoadSave ||
        JourneyRoleForOutcome("respawn_checkpoint") != JourneyBranchRole::Failure ||
        JourneyRoleForOutcome("next") != JourneyBranchRole::Main ||
        JourneyRoleForOutcome("new_game") != JourneyBranchRole::Main ||
        JourneyRoleForOutcome("death", true) != JourneyBranchRole::Detached)
    {
        return Fail("branch and Inspector role classification disagreed");
    }

    std::cout << "PASS: Journey UI fixed shell and role contract\n";
    return 0;
}
