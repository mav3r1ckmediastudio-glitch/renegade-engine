#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace renegade::runtime
{
    enum class RuntimeInputSource
    {
        Mouse,
        Keyboard,
        Gamepad,
        Test,
    };

    enum class RuntimeActionCode
    {
        Success,
        UnknownAction,
        ActionUnavailable,
        FlowStartFailed,
        AlreadyStarted,
        QuitRequested,
    };

    struct RuntimeActionRequest
    {
        std::string actionId;
        std::string widgetId;
        RuntimeInputSource inputSource = RuntimeInputSource::Test;
        std::uint64_t sequence = 0;
    };

    struct RuntimeActionResult
    {
        bool succeeded = false;
        RuntimeActionCode code = RuntimeActionCode::UnknownAction;
        RuntimeActionRequest request;
        std::string message;
    };

    [[nodiscard]] const char* RuntimeInputSourceName(
        RuntimeInputSource source) noexcept;
    [[nodiscard]] const char* RuntimeActionCodeName(
        RuntimeActionCode code) noexcept;

    class RuntimeActionDispatcher
    {
    public:
        using Handler =
            std::function<RuntimeActionResult(const RuntimeActionRequest&)>;

        [[nodiscard]] bool Register(
            std::string actionId,
            Handler handler,
            std::string& error);
        [[nodiscard]] RuntimeActionResult Dispatch(
            const RuntimeActionRequest& request) const;
        void Clear() noexcept;

    private:
        std::unordered_map<std::string, Handler> handlers_;
    };
}
