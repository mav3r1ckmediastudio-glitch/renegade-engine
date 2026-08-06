#include "RuntimeActions.h"
#include "RuntimeScreen.h"

#include "renegade/bridge/ScreenService.h"

#include <iostream>
#include <string>

namespace
{
    using namespace renegade;

    bridge::ScreenDocument MakeScreen()
    {
        bridge::ScreenDocument document;
        const bridge::StableId projectId = bridge::GenerateStableId();
        document.envelope = bridge::CreateDocumentEnvelope(
            projectId,
            bridge::RuntimeScreenDocumentType,
            "Content/UI/Main.renegade-screen",
            "Renegade LP03 Runtime screen tests");
        document.designWidth = 1280.0f;
        document.designHeight = 720.0f;
        document.actions = {
            {bridge::RuntimeScreenPlayAction},
            {bridge::RuntimeScreenQuitAction},
        };
        document.widgets = {
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Image,
                "Background", {0, 0, 1280, 720}, true, false, {},
                "Content/UI/background.png", {}},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Text,
                "Title", {240, 120, 800, 100}, true, false,
                "RENEGADE", {}, {}},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Button,
                "Play", {440, 330, 400, 76}, true, true,
                "PLAY", {}, bridge::RuntimeScreenPlayAction},
            {bridge::GenerateStableId(), bridge::ScreenWidgetKind::Button,
                "Quit", {440, 440, 400, 76}, true, true,
                "QUIT", {}, bridge::RuntimeScreenQuitAction},
        };
        document.focusOrder = {
            document.widgets[2].id,
            document.widgets[3].id,
        };
        return document;
    }

    int Fail(const char* message)
    {
        std::cerr << "RenegadeRuntimeScreenTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade;

    bridge::ScreenDocument document = MakeScreen();
    runtime::RuntimeScreenController controller;
    std::string error;
    if (!controller.Initialize(document, error))
    {
        return Fail("valid controller document was rejected");
    }

    const std::string playId = document.widgets[2].id;
    const std::string quitId = document.widgets[3].id;
    if (controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("initial focus was not deterministic");
    }
    if (!controller.FocusNext() ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != quitId ||
        !controller.FocusPrevious() ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("next/previous focus order was not deterministic");
    }

    runtime::RuntimeActionDispatcher dispatcher;
    int playCount = 0;
    int quitCount = 0;
    if (!dispatcher.Register(
            bridge::RuntimeScreenPlayAction,
            [&playCount](const runtime::RuntimeActionRequest& request)
            {
                ++playCount;
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::Success,
                    request,
                    "play entered LP02 boundary",
                };
            },
            error) ||
        !dispatcher.Register(
            bridge::RuntimeScreenQuitAction,
            [&quitCount](const runtime::RuntimeActionRequest& request)
            {
                ++quitCount;
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::QuitRequested,
                    request,
                    "quit requested normal shutdown",
                };
            },
            error))
    {
        return Fail("could not register bounded actions");
    }

    runtime::RuntimeActionRequest request;
    if (!controller.MakeFocusedActionRequest(
            runtime::RuntimeInputSource::Keyboard,
            1,
            request,
            error))
    {
        return Fail("keyboard activation did not produce an action request");
    }
    auto result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "play" ||
        result.request.inputSource != runtime::RuntimeInputSource::Keyboard ||
        playCount != 1)
    {
        return Fail("keyboard did not enter the stable play dispatcher");
    }

    if (!controller.FocusNext() ||
        !controller.MakeFocusedActionRequest(
            runtime::RuntimeInputSource::Gamepad,
            2,
            request,
            error))
    {
        return Fail("gamepad activation did not produce an action request");
    }
    result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "quit" ||
        result.request.inputSource != runtime::RuntimeInputSource::Gamepad ||
        quitCount != 1)
    {
        return Fail("gamepad did not enter the stable quit dispatcher");
    }

    if (!controller.MakeActionRequest(
            playId,
            runtime::RuntimeInputSource::Mouse,
            3,
            request,
            error))
    {
        return Fail("mouse activation did not produce an action request");
    }
    result = dispatcher.Dispatch(request);
    if (!result.succeeded || result.request.actionId != "play" ||
        result.request.inputSource != runtime::RuntimeInputSource::Mouse ||
        playCount != 2)
    {
        return Fail("mouse did not enter the same stable dispatcher");
    }

    if (!controller.SetWidgetState(playId, false, true, error) ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != quitId)
    {
        return Fail("hidden control was not skipped and focus repaired");
    }
    if (!controller.SetWidgetState(quitId, true, false, error) ||
        controller.HasFocus())
    {
        return Fail("disabled controls were not skipped");
    }
    if (!controller.SetWidgetState(playId, true, true, error) ||
        controller.FocusedWidget() == nullptr ||
        controller.FocusedWidget()->id != playId)
    {
        return Fail("focus did not recover when a control became available");
    }
    if (!controller.SetWidgetState(playId, false, true, error) ||
        controller.MakeActionRequest(
            playId,
            runtime::RuntimeInputSource::Mouse,
            4,
            request,
            error))
    {
        return Fail("hidden control activation did not fail closed");
    }

    runtime::RuntimeActionRequest unknown;
    unknown.actionId = "unknown.action";
    unknown.widgetId = playId;
    unknown.inputSource = runtime::RuntimeInputSource::Test;
    unknown.sequence = 5;
    result = dispatcher.Dispatch(unknown);
    if (result.succeeded ||
        result.code != runtime::RuntimeActionCode::UnknownAction ||
        result.message.find("Unknown Runtime action") == std::string::npos)
    {
        return Fail("unknown action did not produce a structured failure");
    }

    if (dispatcher.Register(
            bridge::RuntimeScreenPlayAction,
            [](const runtime::RuntimeActionRequest& request)
            {
                return runtime::RuntimeActionResult{
                    true,
                    runtime::RuntimeActionCode::Success,
                    request,
                    {},
                };
            },
            error))
    {
        return Fail("duplicate action registration was accepted");
    }

    std::cout << "PASS: LP03 Runtime focus and stable action dispatch\n";
    return 0;
}
