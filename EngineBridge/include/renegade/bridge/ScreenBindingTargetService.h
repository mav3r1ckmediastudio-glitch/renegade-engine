#pragma once
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct ScreenBindingActorChoice
    {
        StableId id;
        std::string name;
    };
    struct ScreenBindingActorCatalogue
    {
        std::vector<ScreenBindingActorChoice> characters;
        std::vector<std::string> diagnostics;
    };
    // Read-only authoring projection of the currently loaded Level. Never
    // assigns identities, spawns actors, or guesses a Player from a name.
    inline ScreenBindingActorCatalogue CollectScreenBindingActors(const wi::scene::Scene& scene)
    {
        ScreenBindingActorCatalogue catalogue;
        std::map<StableId,std::vector<ScreenBindingActorChoice>> byIdentity;
        for(const auto entity:CollectCharacters(scene))
        {
            const auto identity=PersistentEntityId(scene,entity);
            const auto* named=scene.names.GetComponent(entity);
            const auto name=named && !named->name.empty()?named->name:std::string("Unnamed Character");
            if(!IsValidStableId(identity))
            {catalogue.diagnostics.push_back(name+": Character has no valid persistent identity.");continue;}
            byIdentity[identity].push_back({identity,name});
        }
        for(auto& entry:byIdentity)
        {
            if(entry.second.size()!=1)
            {catalogue.diagnostics.push_back("Duplicate Character identity: "+entry.first);continue;}
            catalogue.characters.push_back(std::move(entry.second.front()));
        }
        std::sort(catalogue.characters.begin(),catalogue.characters.end(),[](const auto& a,const auto& b){
            return a.name!=b.name?a.name<b.name:a.id<b.id;});
        return catalogue;
    }
}
