#include "renegade/bridge/FirstPersonAssemblyService.h"
#include <iostream>
#include <limits>
using namespace renegade::bridge;
int main()
{
    FirstPersonAssemblySettings settings;
    settings.armsAssetId = GenerateStableId();
    settings.weaponAssetId = GenerateStableId();
    settings.parentBonePath = "[\"root\",\"grip\"]";
    settings.weaponPosition = {-0.03466970f, 0.27336276f, -0.04505738f};
    settings.weaponRotation = {-0.059182247f, 0.087738050f, 0.994176416f, -0.020316230f};
    settings.cameraPosition = {0, -1.65f, 0.08f};
    settings.cameraRotation = {0, 0.707106781f, -0.707106781f, 0};
    settings.pairs = {{"Idle", 0, 1}, {"Reload", 2, 3}};
    std::string json, reopened, error;
    FirstPersonAssemblySettings parsed;
    if (!SerializeFirstPersonAssemblySettings(settings, json, error) ||
        !ParseFirstPersonAssemblySettings(json, parsed, error) ||
        !SerializeFirstPersonAssemblySettings(parsed, reopened, error) || reopened != json)
    { std::cerr << "Assembly attachment or paired indices lost on reopen: " << error; return 1; }
    for (int failure = 0; failure < 7; ++failure)
    {
        auto bad = settings;
        if (failure == 0) bad.parentBonePath = "[1]";
        if (failure == 1) bad.weaponAssetId = bad.armsAssetId;
        if (failure == 2) bad.weaponRotation.w = 5;
        if (failure == 3) bad.cameraPosition.x = std::numeric_limits<float>::quiet_NaN();
        if (failure == 4) bad.pairs.push_back(bad.pairs.front());
        if (failure == 5) bad.pairs[0].action = "GuessFromFilename";
        if (failure == 6) bad.pairs.clear();
        if (SerializeFirstPersonAssemblySettings(bad, reopened, error))
        { std::cerr << "Invalid assembly accepted: " << failure; return 2; }
    }
    if (ParseFirstPersonAssemblySettings("{}", parsed, error) ||
        ParseFirstPersonAssemblySettings("{\"schema_version\":2}", parsed, error))
    { std::cerr << "Unsupported assembly recipe accepted."; return 3; }
    std::cout << "Assembly retained attachment, paired indices and invalid-input checks pass.\n";
    return 0;
}
