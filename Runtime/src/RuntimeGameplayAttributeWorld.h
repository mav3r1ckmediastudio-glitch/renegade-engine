#pragma once
#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include <memory>
#include <set>
namespace renegade::runtime
{
    struct GameplayAttributeActorLifetime
    {
        bridge::StableId id;
        std::uint64_t entity=0;
    };
    // Optional gameplay owner. It exists independently of Screen/HUD requests.
    class RuntimeGameplayAttributeWorld
    {
    public:
        bool Configure(const bridge::GameplayAttributeDocument& document,bool reset,std::string& error)
        {
            if(!bridge::ValidateGameplayAttributeDocument(document,document.envelope.projectId,error))return false;
            auto next=*this;next.document_=document;next.configured_=true;
            if(reset){next.project_.reset();next.player_.reset();next.characters_.clear();next.lifetimes_.clear();next.playerEntity_=next.playerScene_=0;}
            const auto* project=next.Attachment(bridge::GameplayAttributeScope::Project,"");
            if(project)
            {
                if(!next.project_)next.project_=bridge::GameplayAttributeInstance{};
                if(!next.Apply(*next.project_,*project,reset,error))return false;
            }
            else next.project_.reset();
            if(next.player_)
            {
                const auto* attachment=next.Attachment(bridge::GameplayAttributeScope::Player,"");
                if(!attachment)next.player_.reset();else if(!next.Apply(*next.player_,*attachment,reset,error))return false;
            }
            for(auto it=next.characters_.begin();it!=next.characters_.end();)
            {
                const auto* attachment=next.Attachment(bridge::GameplayAttributeScope::Character,it->first);
                if(!attachment){next.lifetimes_.erase(it->first);it=next.characters_.erase(it);}
                else{if(!next.Apply(it->second,*attachment,reset,error))return false;++it;}
            }
            *this=std::move(next);error.clear();return true;
        }
        bool SyncActors(std::uint64_t sceneGeneration,std::uint64_t playerEntity,
            const std::vector<GameplayAttributeActorLifetime>& actors,std::string& error)
        {
            if(!configured_){error.clear();return true;}
            std::map<bridge::StableId,std::pair<std::uint64_t,std::uint64_t>> expected;
            std::set<bridge::StableId> seen;
            for(const auto& actor:actors)
            {
                if(sceneGeneration==0||actor.entity==0||!bridge::IsValidStableId(actor.id)||!seen.insert(actor.id).second)
                {error="Gameplay attribute actor lifetime is invalid or duplicated.";return false;}
                if(Attachment(bridge::GameplayAttributeScope::Character,actor.id))expected[actor.id]={sceneGeneration,actor.entity};
            }
            const bool wantsPlayer=Attachment(bridge::GameplayAttributeScope::Player,"")&&sceneGeneration!=0&&playerEntity!=0;
            const bool samePlayer=wantsPlayer?(player_&&playerEntity_==playerEntity&&playerScene_==sceneGeneration):!player_;
            if(samePlayer&&expected==lifetimes_){error.clear();return true;}
            auto next=*this;
            const auto* player=Attachment(bridge::GameplayAttributeScope::Player,"");
            if(!player||sceneGeneration==0||playerEntity==0)
            {next.player_.reset();next.playerEntity_=next.playerScene_=0;}
            else if(!next.player_||next.playerEntity_!=playerEntity||next.playerScene_!=sceneGeneration)
            {
                bridge::GameplayAttributeInstance instance;if(!Apply(instance,*player,true,error))return false;
                next.player_=std::move(instance);next.playerEntity_=playerEntity;next.playerScene_=sceneGeneration;
            }
            std::set<bridge::StableId> active;
            for(const auto& actor:actors)
            {
                if(sceneGeneration==0||actor.entity==0||!bridge::IsValidStableId(actor.id)||!active.insert(actor.id).second)
                {error="Gameplay attribute actor lifetime is invalid or duplicated.";return false;}
                const auto* attachment=Attachment(bridge::GameplayAttributeScope::Character,actor.id);if(!attachment)continue;
                const auto old=next.lifetimes_.find(actor.id);
                if(old==next.lifetimes_.end()||old->second.first!=sceneGeneration||old->second.second!=actor.entity)
                {
                    bridge::GameplayAttributeInstance instance;if(!Apply(instance,*attachment,true,error))return false;
                    next.characters_[actor.id]=std::move(instance);next.lifetimes_[actor.id]={sceneGeneration,actor.entity};
                }
            }
            for(auto it=next.characters_.begin();it!=next.characters_.end();)
                if(!active.count(it->first)){next.lifetimes_.erase(it->first);it=next.characters_.erase(it);}else ++it;
            *this=std::move(next);error.clear();return true;
        }
        bool Tick(double dt,bool paused,std::string& error)
        {
            if(!configured_){error.clear();return true;}
            if(!std::isfinite(dt)||dt<0||dt>60){error="Gameplay attribute delta must be finite and bounded.";return false;}
            if(paused||dt==0){error.clear();return true;}
            auto project=project_,player=player_;auto characters=characters_;
            if(project&&!project->Tick(dt,false,error))return false;
            if(player&&!player->Tick(dt,false,error))return false;
            for(auto& entry:characters)if(!entry.second.Tick(dt,false,error))return false;
            project_=std::move(project);player_=std::move(player);characters_=std::move(characters);error.clear();return true;
        }
        void Clear() noexcept{*this={};}
        bridge::GameplayAttributeInstance* Project() noexcept{return project_?&*project_:nullptr;}
        bridge::GameplayAttributeInstance* Player() noexcept{return player_?&*player_:nullptr;}
        const bridge::GameplayAttributeInstance* Project() const noexcept{return project_?&*project_:nullptr;}
        const bridge::GameplayAttributeInstance* Player() const noexcept{return player_?&*player_:nullptr;}
        const std::map<bridge::StableId,bridge::GameplayAttributeInstance>& Characters() const noexcept{return characters_;}
        bridge::GameplayAttributeInstance* Character(const bridge::StableId& id) noexcept
        {const auto it=characters_.find(id);return it==characters_.end()?nullptr:&it->second;}
    private:
        const bridge::GameplayAttributeAttachment* Attachment(bridge::GameplayAttributeScope scope,const bridge::StableId& id) const
        {
            const auto found=std::find_if(document_.attachments.begin(),document_.attachments.end(),
                [&](const auto& value){return value.scope==scope&&value.targetId==id;});
            return found==document_.attachments.end()?nullptr:&*found;
        }
        bool Apply(bridge::GameplayAttributeInstance& instance,const bridge::GameplayAttributeAttachment& attachment,bool reset,std::string& error) const
        {
            const auto profile=std::find_if(document_.profiles.begin(),document_.profiles.end(),
                [&](const auto& value){return value.id==attachment.profileId;});
            if(profile==document_.profiles.end()){error="Gameplay attribute profile disappeared.";return false;}
            return instance.ApplyProfile(*profile,attachment.overrides,reset,error);
        }
        bridge::GameplayAttributeDocument document_;
        std::optional<bridge::GameplayAttributeInstance> project_,player_;
        std::map<bridge::StableId,bridge::GameplayAttributeInstance> characters_;
        std::map<bridge::StableId,std::pair<std::uint64_t,std::uint64_t>> lifetimes_;
        std::uint64_t playerEntity_=0,playerScene_=0;bool configured_=false;
    };
}
