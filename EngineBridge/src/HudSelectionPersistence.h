#pragma once
#include "renegade/bridge/HudSelection.h"
#include <wiConfig.h>
#include <sstream>
namespace renegade::bridge::detail
{
    inline bool ValidateLevelHudSelection(const LevelHudSelection& selection,std::string& error)
    {
        if(selection.mode!=LevelHudMode::InheritProject && selection.mode!=LevelHudMode::None &&
            selection.mode!=LevelHudMode::Override)
        {error="Unknown Level HUD mode.";return false;}
        if((selection.mode==LevelHudMode::Override)!=selection.hud.has_value())
        {error="Level HUD override mode/reference do not match.";return false;}
        if(selection.hud && !ValidateHudReference(*selection.hud,error))return false;
        error.clear();return true;
    }
    inline void WriteLevelHudSelection(wi::config::Section& section,const LevelHudSelection& selection)
    {
        section.Set("hud_mode",static_cast<int>(selection.mode));
        section.Set("hud_document_id",selection.hud?selection.hud->documentId:std::string{});
        section.Set("hud_path_hint",selection.hud?selection.hud->pathHint:std::string{});
    }
    inline bool ReadLevelHudSelection(const wi::config::Section& section,LevelHudSelection& selection,std::string& error)
    {
        if(!section.Has("hud_mode") && !section.Has("hud_document_id") && !section.Has("hud_path_hint"))
        {selection={};error.clear();return true;}
        if(!section.Has("hud_mode") || !section.Has("hud_document_id") || !section.Has("hud_path_hint"))
        {error="Level HUD fields are incomplete.";return false;}
        int mode=0;std::istringstream text(section.GetText("hud_mode"));text.imbue(std::locale::classic());
        text>>std::noskipws>>mode;
        if(text.fail() || text.peek()!=std::char_traits<char>::eof())
        {error="Level HUD mode must be an explicit integer.";return false;}
        LevelHudSelection candidate;candidate.mode=static_cast<LevelHudMode>(mode);
        const auto identity=section.GetText("hud_document_id"),path=section.GetText("hud_path_hint");
        if(!identity.empty() || !path.empty())candidate.hud=HudReference{identity,path};
        if(!ValidateLevelHudSelection(candidate,error))return false;
        selection=std::move(candidate);return true;
    }
}
