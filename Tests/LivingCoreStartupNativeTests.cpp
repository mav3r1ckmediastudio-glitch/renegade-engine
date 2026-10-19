#include "../Studio/src/StartupIdentityPrompt.h"
#include "../Studio/src/StudioUserPreferences.h"
#include "../Studio/src/RenegadeLivingPresenceLaunch.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
int main()
{
    using namespace renegade::studio;
    wchar_t temp[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, temp)) return 1;
    const auto root = std::filesystem::path(temp) /
        (L"RenegadeStartupProof-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    if (!SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str())) return 2;
    if (StudioUserPreferences::LoadDeveloperIdentity()) return 3;
    const HWND parent = CreateWindowExW(0, L"STATIC", L"Renegade startup proof",
        WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!parent) return 4;
    StartupIdentityPrompt prompt;
    if (!prompt.Start(parent)) return 5;
    PublishLivingCoreStartup("name");
    const auto marker = StudioUserPreferences::PreferencesPath().parent_path() /
        L"living-core-startup.cfg";
    std::ifstream input(marker);
    std::string text((std::istreambuf_iterator<char>(input)), {});
    input.close();
    if (text.find("stage=name") == std::string::npos) return 6;
    if (!StudioUserPreferences::SaveDeveloperIdentity(L"Zoë")) return 7;
    prompt.Pump();
    if (!prompt.IsCompleted() || prompt.IsActive() || prompt.Identity() != L"Zoë") return 8;
    prompt.Shutdown(); DestroyWindow(parent);
    PublishLivingCoreStartup(nullptr);
    if (std::filesystem::exists(marker)) return 9;
    if (StudioUserPreferences::LoadDeveloperIdentity() != std::optional<std::wstring>(L"Zoë")) return 10;
    std::filesystem::remove_all(root);
    std::cout << "PASS native external name acceptance, canonical preference reopen and startup marker cleanup\n";
    return 0;
}
