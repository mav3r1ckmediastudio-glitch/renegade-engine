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
    CommandService history;
    auto draft=settings;
    auto next=draft;next.weaponPosition.x+=0.01f;next.cameraPosition.z+=0.02f;
    next.cameraRotation={0,0,0,1};next.pairs.pop_back();
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,next))||
        !history.IsDirty()||draft.pairs.size()!=1||!history.Undo()||
        !SerializeFirstPersonAssemblySettings(draft,reopened,error)||reopened!=json||
        history.IsDirty()||!history.Redo()||draft.cameraPosition.z!=next.cameraPosition.z)
        return 4;
    history.MarkSaved();
    if(history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,draft))||
        history.IsDirty()||!history.Undo()||!history.IsDirty()||!history.Redo()||history.IsDirty())
        return 5;
    auto incomplete=draft;incomplete.pairs[0].weaponClip=~0u;
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,incomplete))||
        !history.Undo()||draft.pairs[0].weaponClip!=next.pairs[0].weaponClip)
        return 6;
    auto branch=draft;branch.weaponPosition.y+=0.01f;
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,branch))||
        history.CanRedo())
        return 7;
    auto full=settings;full.pairs.clear();
    for(unsigned i=0;i<FirstPersonAssemblyActions.size();++i)full.pairs.push_back({FirstPersonAssemblyActions[i],i,0});
    if(!SerializeFirstPersonAssemblySettings(full,reopened,error)||
        !ParseFirstPersonAssemblySettings(reopened,parsed,error)||parsed.pairs.size()!=14)return 8;
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
