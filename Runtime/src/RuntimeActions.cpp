#include "RuntimeActions.h"

#include <algorithm>
#include <utility>

namespace
{
    bool IsValidActionId(const std::string& value)
    {
        if (value.empty() || value.front() < 'a' || value.front() > 'z')
        {
            return false;
        }
        return std::all_of(
            value.begin(),
            value.end(),
            [](const unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= '0' && character <= '9') ||
                    character == '.' || character == '_' || character == '-';
            });
    }
}

namespace renegade::runtime
{
    const char* RuntimeInputSourceName(const RuntimeInputSource source) noexcept
    {
        switch (source)
        {
        case RuntimeInputSource::Mouse:
            return "mouse";
        case RuntimeInputSource::Keyboard:
            return "keyboard";
        case RuntimeInputSource::Gamepad:
            return "gamepad";
        case RuntimeInputSource::Test:
            return "test";
        default:
            return "unknown";
        }
    }

    const char* RuntimeActionCodeName(const RuntimeActionCode code) noexcept
    {
        switch (code)
        {
        case RuntimeActionCode::Success:
            return "success";
        case RuntimeActionCode::UnknownAction:
            return "unknown_action";
        case RuntimeActionCode::ActionUnavailable:
            return "action_unavailable";
        case RuntimeActionCode::FlowStartFailed:
            return "flow_start_failed";
        case RuntimeActionCode::AlreadyStarted:
            return "already_started";
        case RuntimeActionCode::QuitRequested:
            return "quit_requested";
        default:
            return "unknown";
        }
    }

    bool RuntimeActionDispatcher::Register(
        std::string actionId,
        Handler handler,
        std::string& error)
    {
        if (!IsValidActionId(actionId))
        {
            error = "Runtime action registration requires a valid symbolic ID.";
            return false;
        }
        if (!handler)
        {
            error = "Runtime action registration requires a handler.";
            return false;
        }
        if (handlers_.count(actionId) != 0)
        {
            error = "Runtime action already registered: " + actionId;
            return false;
        }

        handlers_.emplace(std::move(actionId), std::move(handler));
        error.clear();
        return true;
    }

    RuntimeActionResult RuntimeActionDispatcher::Dispatch(
        const RuntimeActionRequest& request) const
    {
        const auto found = handlers_.find(request.actionId);
        if (found == handlers_.end())
        {
            return {
                false,
                RuntimeActionCode::UnknownAction,
                request,
                "Unknown Runtime action: " + request.actionId,
            };
        }

        RuntimeActionResult result = found->second(request);
        result.request = request;
        return result;
    }

    void RuntimeActionDispatcher::Clear() noexcept
    {
        handlers_.clear();
    }
}
