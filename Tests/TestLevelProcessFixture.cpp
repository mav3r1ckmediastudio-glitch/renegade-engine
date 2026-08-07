#include <Windows.h>

#include <string>

namespace
{
    std::wstring ArgumentValue(
        const int argc,
        wchar_t** argv,
        const std::wstring& prefix)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::wstring argument = argv[index];
            if (argument.rfind(prefix, 0) == 0)
            {
                return argument.substr(prefix.size());
            }
        }
        return {};
    }

    bool SignalReady(const std::wstring& eventName)
    {
        if (eventName.empty())
        {
            return false;
        }
        HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (event == nullptr)
        {
            return false;
        }
        const bool signaled = SetEvent(event) != FALSE;
        CloseHandle(event);
        return signaled;
    }
}

int wmain(const int argc, wchar_t** argv)
{
    const std::wstring fixture =
        ArgumentValue(argc, argv, L"--fixture=");
    const std::wstring readyEvent =
        ArgumentValue(argc, argv, L"--renegade-ready-event=");

    if (fixture == L"never-ready")
    {
        Sleep(30000);
        return 0;
    }
    if (fixture == L"runtime-failure-before-ready")
    {
        return 24;
    }

    if (!SignalReady(readyEvent))
    {
        return 91;
    }

    if (fixture == L"ready-exit0")
    {
        Sleep(50);
        return 0;
    }
    if (fixture == L"ready-abnormal")
    {
        return 99;
    }
    if (fixture == L"ready-hang")
    {
        Sleep(30000);
        return 0;
    }

    return 92;
}
