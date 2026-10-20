#pragma once
#include "renegade/bridge/IdentityService.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct HudReference { StableId documentId; std::string pathHint; };
    enum class LevelHudMode { InheritProject, None, Override };
    struct LevelHudSelection
    {
        LevelHudMode mode=LevelHudMode::InheritProject;
        std::optional<HudReference> hud;
    };
    inline bool ValidateHudReference(const HudReference& reference,std::string& error)
    {
        if(!IsValidStableId(reference.documentId) || reference.pathHint.empty() || reference.pathHint.size()>1024 ||
            reference.pathHint.find_first_of("\r\n\0",0,3)!=std::string::npos ||
            reference.pathHint.find('\\')!=std::string::npos || reference.pathHint.find(':')!=std::string::npos ||
            reference.pathHint.front()=='/' || reference.pathHint.rfind("Content/",0)!=0)
        {error="HUD requires a stable Screen identity and safe project-relative path.";return false;}
        std::size_t start=0;
        while(start<=reference.pathHint.size())
        {
            const auto end=reference.pathHint.find('/',start);
            const auto part=reference.pathHint.substr(start,end==std::string::npos?end:end-start);
            if(part.empty() || part=="." || part=="..")
            {error="HUD path contains an unsafe component.";return false;}
            if(end==std::string::npos)break;start=end+1;
        }
        constexpr const char* suffix=".renegade-screen";
        if(reference.pathHint.size()<16 || reference.pathHint.compare(reference.pathHint.size()-16,16,suffix)!=0)
        {error="HUD reference must name a Screen document.";return false;}
        error.clear();return true;
    }

    // Pure selection authority: no scene replacement, file reads or gameplay
    // values. Runtime eligibility must reject absent, disabled or non-HUD Screens.
    class HudSelectionController
    {
    public:
        using Eligible=std::function<bool(const HudReference&)>;
        bool EnterLevel(std::uint64_t session,std::optional<HudReference> project,
            LevelHudSelection level,std::string& error)
        {
            if(session==0 || session<=lastLevelSession_)
            {error="HUD Level session must be a fresh monotonic identity.";return false;}
            if(project && !ValidateHudReference(*project,error))return false;
            if(level.mode!=LevelHudMode::InheritProject && level.mode!=LevelHudMode::None && level.mode!=LevelHudMode::Override)
            {error="Unknown Level HUD selection mode.";return false;}
            if((level.mode==LevelHudMode::Override)!=level.hud.has_value())
            {error="Only a Level HUD override may carry a Screen reference.";return false;}
            if(level.hud && !ValidateHudReference(*level.hud,error))return false;
            project_=std::move(project);level_=std::move(level);contexts_.clear();
            levelSession_=lastLevelSession_=session;++revision_;error.clear();return true;
        }
        bool PushContext(std::uint64_t session,std::uint64_t token,std::string reason,
            HudReference hud,std::string& error)
        {
            if(session!=levelSession_ || session==0 || token==0 || token<=lastContextToken_ ||
                contexts_.size()>=32 || reason.empty() || reason.size()>128)
            {error="HUD context has an invalid session, token, reason or count.";return false;}
            if(!ValidateHudReference(hud,error))return false;
            contexts_.push_back({token,std::move(reason),std::move(hud)});
            lastContextToken_=token;++revision_;error.clear();return true;
        }
        bool PopContext(std::uint64_t session,std::uint64_t token,std::string& error)
        {
            if(session==0 || session!=levelSession_)
            {error="HUD exit belongs to an old Level session.";return false;}
            const auto found=std::find_if(contexts_.begin(),contexts_.end(),[token](const auto& item){return item.token==token;});
            if(found==contexts_.end()){error="HUD exit references an absent context.";return false;}
            contexts_.erase(found);++revision_;error.clear();return true;
        }
        std::optional<HudReference> Resolve(const Eligible& eligible) const
        {
            if(levelSession_==0 || !eligible)return std::nullopt;
            for(auto context=contexts_.rbegin();context!=contexts_.rend();++context)
                if(eligible(context->hud))return context->hud;
            if(level_.mode==LevelHudMode::None)return std::nullopt;
            if(level_.mode==LevelHudMode::Override && level_.hud && eligible(*level_.hud))return level_.hud;
            if(project_ && eligible(*project_))return project_;
            return std::nullopt;
        }
        void LeaveLevel() noexcept
        {levelSession_=0;project_.reset();level_={};contexts_.clear();++revision_;}
        std::uint64_t Revision() const noexcept{return revision_;}
        std::uint64_t LevelSession() const noexcept{return levelSession_;}
    private:
        struct Context{std::uint64_t token;std::string reason;HudReference hud;};
        std::optional<HudReference> project_;
        LevelHudSelection level_;
        std::vector<Context> contexts_;
        std::uint64_t levelSession_=0,lastLevelSession_=0,lastContextToken_=0,revision_=0;
    };
}
