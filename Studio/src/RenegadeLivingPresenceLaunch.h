#pragma once
// RA-03 developer preview: explicitly launch the locally installed Living Core.
// The 9-Oct approved V1 renderer runs in its own GPU-backed transparent window.
// This launcher has NO editor-mutation APIs and does not touch any game project.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <fstream>
#include "StudioUserPreferences.h"
#pragma comment(lib, "Shell32.lib")

namespace renegade::studio
{
    inline void PublishLivingCoreStartup(const char* stage)
    {
        const auto preferences = StudioUserPreferences::PreferencesPath();
        if (preferences.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(preferences.parent_path(), error);
        if (error) return;
        const auto request = preferences.parent_path() / L"living-core-startup.cfg";
        if (stage == nullptr) { std::filesystem::remove(request, error); return; }
        auto temporary = request; temporary += L".tmp";
        { std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
          output << "stage=" << stage << "\npid=" << GetCurrentProcessId() << '\n';
          output.flush(); if (!output) return; }
        MoveFileExW(temporary.c_str(), request.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }

    inline bool LaunchLivingCoreFromStudio(bool quiet = false)
    {
        wchar_t exe[MAX_PATH] = {};
        const DWORD len = ::GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!len || len >= MAX_PATH)
        {
            if (quiet) return false;
            ::MessageBoxW(nullptr, L"Could not locate the Studio executable.",
                L"Renegade Living Core", MB_OK | MB_ICONWARNING);
            return false;
        }
        std::filesystem::path folder = std::filesystem::path(exe).parent_path();
        // Developer layout: BUILD/renegade/Studio/{Debug,Release}/RenegadeStudio.exe.
        // Future packages may put Tools beside Studio; search parents, not CWD.
        for (int depth=0; depth<9; ++depth)
        {
            const auto launcher=folder / L"Tools" / L"assistant_presence" /
                L"START_RENEGADE_PRESENCE.cmd";
            if (std::filesystem::is_regular_file(launcher))
            {
                // Explicit user command; sidecar exits safely if a previous orb
                // is already running. No model process is killed.
                const auto result=reinterpret_cast<INT_PTR>(::ShellExecuteW(
                    nullptr,L"open",launcher.c_str(),nullptr,folder.c_str(),quiet ? SW_HIDE : SW_SHOWNORMAL));
                if (result>32)return true;
                if (quiet) return false;
                ::MessageBoxW(nullptr,
                    L"Could not launch Living Core. Check the local Windows/Electron runtime.",
                    L"Renegade Living Core",MB_OK | MB_ICONWARNING);
                return false;
            }
            if (!folder.has_parent_path() || folder==folder.parent_path())
                break;
            folder=folder.parent_path();
        }
        if (quiet) return false;
        ::MessageBoxW(nullptr,
            L"The Living Core developer companion is not installed beside Renegade. "
            L"Use Tools/assistant_presence/START_RENEGADE_PRESENCE.cmd "
            L"from the source checkout or complete the packaging step.",
            L"Renegade Living Core",MB_OK | MB_ICONINFORMATION);
        return false;
    }
}
