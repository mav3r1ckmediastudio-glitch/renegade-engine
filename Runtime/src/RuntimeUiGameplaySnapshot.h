#pragma once
#include "RuntimeCombatService.h"
#include "RuntimePlayerViewAnimation.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ScreenBindingFrame.h"
#include "renegade/bridge/PlayerWeaponAmmo.h"
#include <set>
#include <tuple>

namespace renegade::runtime
{
    inline bridge::PlayerWeaponAmmo CapturePlayerWeaponAmmo(
        const RuntimePlayerViewAnimationState* view,const std::string& name,unsigned slot)
    {
        bridge::PlayerWeaponAmmo result;
        if(!view||!view->initialized)return result;
        result.equipped=view->equipped;result.name=name;result.slot=slot;
        result.available=view->pairedAssembly&&!view->meleeWeapon&&!view->spellPresentation&&view->equipped;
        if(result.available){
            result.loaded=view->loadedShells;result.capacity=view->firearm.capacity;
            result.reloading=view->reloadStagePlaying||(view->oneShotPlaying&&
                (view->activeAction==PlayerViewAction::Reload||view->activeAction==PlayerViewAction::ReloadPartial));
        }
        return result;
    }

    struct RuntimeUiGameplaySources
    {
        const bridge::RuntimePlayerState* player = nullptr;
        const wi::scene::Scene* scene = nullptr;
        const RuntimeCombatState* combat = nullptr;
        const RuntimeCharacterSystemState* characters = nullptr;
        const RuntimePlayerViewAnimationState* playerView = nullptr;
        bool combatReady = false;
        std::uint64_t sceneGeneration = 0;
        const bridge::GameplayAttributeInstance* playerAttributes = nullptr;
        const bridge::GameplayAttributeInstance* projectAttributes = nullptr;
        const std::map<bridge::StableId, bridge::GameplayAttributeInstance>* characterAttributes = nullptr;
        std::string weaponName,weaponSlots;
        unsigned weaponSlot=0;
    };

    // Read-only adapters over actual gameplay owners. It never creates health,
    // ammo, equipment state or profile instances to satisfy a UI request.
    class RuntimeUiSnapshotPublisher
    {
    public:
        bool BeginSession(std::uint64_t session, std::string& error)
        {
            if (session == 0 || session <= lastSessionIdentity_)
            { error = "UI snapshot session must be a new nonzero identity."; return false; }
            session_ = lastSessionIdentity_ = session;
            published_ = false;
            lifetimes_.clear();
            error.clear();
            return true;
        }

        void Stop() noexcept
        {
            session_ = 0;
            published_ = false;
            lifetimes_.clear();
        }

        bool Publish(const std::vector<bridge::ScreenBindingSource>& requests,
            std::uint64_t revision, const RuntimeUiGameplaySources& sources,
            bridge::ScreenBindingSnapshot& snapshot, bridge::ScreenBindingContext& context,
            std::string& error)
        {
            if (session_ == 0 || requests.size() > 4096 ||
                (published_ && revision <= lastRevision_))
            { error = "UI snapshots require an active session, bounded sources and a newer gameplay revision."; return false; }
            std::set<std::tuple<int,bridge::StableId,std::string>> seen;
            bridge::ScreenBindingSnapshot next;
            bridge::ScreenBindingContext nextContext;
            next.session = nextContext.session = session_;
            next.revision = revision;
            auto nextLifetimes = lifetimes_;
            auto nextGeneration = nextGeneration_;
            std::set<bridge::ScreenBindingContext::Target> active;
            for (const auto& source : requests)
            {
                if (!bridge::ValidateScreenBindingSource(source,error)) return false;
                if (!seen.emplace(static_cast<int>(source.scope),source.targetId,source.attributeId).second)
                    continue;
                using Scope = bridge::ScreenBindingScope;
                const bridge::ScreenBindingContext::Target target{source.scope,source.targetId};
                const bridge::GameplayAttributeValue* custom = nullptr;
                bridge::GameplayAttributeValue value;
                bool available = false;
                wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
                if (source.scope == Scope::PossessedPlayer)
                {
                    if (!sources.player || !sources.player->IsSpawned() || sources.sceneGeneration == 0)
                        continue;
                    entity = sources.player->entity;
                    if(sources.scene)
                    {
                        const auto* body=sources.scene->rigidbodies.GetComponent(entity);
                        if(!sources.scene->transforms.GetComponent(entity) || !body || !body->IsCharacterPhysics())continue;
                    }
                    if (sources.combatReady && sources.combat)
                    {
                        if (source.attributeId == "health.current") { value = double(sources.combat->playerHealth); available = true; }
                        else if (source.attributeId == "health.maximum") { value = double(sources.combat->playerMaxHealth); available = true; }
                        else if (source.attributeId == "alive") { value = !sources.combat->playerDead; available = true; }
                    }
                    const auto ammo=CapturePlayerWeaponAmmo(sources.playerView,sources.weaponName,sources.weaponSlot);
                    if(source.attributeId=="ammo.available"){value=ammo.available;available=true;}
                    if (sources.playerView && sources.playerView->initialized)
                    {
                        if(source.attributeId=="equipment.name" && !sources.weaponName.empty()){value=sources.weaponName;available=true;}
                        if(source.attributeId=="equipment.slots" && !sources.weaponSlots.empty()){value=sources.weaponSlots;available=true;}
                        if (source.attributeId == "equipment.equipped")
                        { value = sources.playerView->equipped; available = true; }
                        if(source.attributeId=="equipment.slot"&&ammo.slot>0){value=double(ammo.slot);available=true;}
                        if (ammo.available)
                        {
                            if (source.attributeId == "ammo.loaded")
                            { value = double(ammo.loaded); available = true; }
                            else if (source.attributeId == "ammo.capacity")
                            { value = double(ammo.capacity); available = true; }
                            else if(source.attributeId=="ammo.reloading"){value=ammo.reloading;available=true;}
                        }
                    }
                    if (!available && !IsNativeAttribute(source.attributeId) && sources.playerAttributes)
                        custom = sources.playerAttributes->Find(source.attributeId);
                }
                else if (source.scope == Scope::Character)
                {
                    const auto* character = sources.combatReady && sources.combat ?
                        FindCharacterCombat(*sources.combat,source.targetId) : nullptr;
                    const auto* actor = sources.characters ?
                        FindRuntimeCharacter(*sources.characters,source.targetId) : nullptr;
                    if (sources.characters && !actor) continue;
                    if (actor && character && actor->entity != character->entity) character = nullptr;
                    entity = actor ? actor->entity : (character ? character->entity : wi::ecs::INVALID_ENTITY);
                    if (entity == wi::ecs::INVALID_ENTITY || sources.sceneGeneration == 0 ||
                        (sources.scene && !sources.scene->characters.GetComponent(entity))) continue;
                    if (character)
                    {
                    if (source.attributeId == "health.current") { value = double(character->health); available = true; }
                    else if (source.attributeId == "health.maximum") { value = double(character->maxHealth); available = true; }
                    else if (source.attributeId == "alive") { value = !character->dead; available = true; }
                    else if (UsesAmmunition(*character))
                    {
                        if (source.attributeId == "ammo.loaded") { value = double(character->magazineAmmo); available = true; }
                        else if (source.attributeId == "ammo.capacity") { value = double(character->weapon.magazineSize); available = true; }
                        else if (source.attributeId == "ammo.reserve") { value = double(character->reserveAmmo); available = true; }
                        else if (source.attributeId == "ammo.reloading") { value = character->reloadRemainingSeconds > 0; available = true; }
                    }
                    }
                    if (!available && !IsNativeAttribute(source.attributeId) && sources.characterAttributes)
                    {
                        const auto found = sources.characterAttributes->find(source.targetId);
                        if (found != sources.characterAttributes->end()) custom = found->second.Find(source.attributeId);
                    }
                }
                else if (sources.projectAttributes) custom = sources.projectAttributes->Find(source.attributeId);

                std::uint64_t generation = 0;
                if (source.scope != Scope::ProjectAttribute)
                {
                    active.insert(target);
                    auto& lifetime = nextLifetimes[target];
                    if (lifetime.entity != entity || lifetime.scene != sources.sceneGeneration || lifetime.generation == 0)
                    {
                        if (nextGeneration == std::numeric_limits<std::uint64_t>::max())
                        { error = "UI entity generation space is exhausted."; return false; }
                        lifetime = {entity,sources.sceneGeneration,++nextGeneration};
                    }
                    generation = lifetime.generation;
                    nextContext.generations.emplace(target,generation);
                }
                if (custom) { value = *custom; available = true; }
                if (available) next.values.push_back({source,std::move(value),generation});
            }
            for (auto it = nextLifetimes.begin(); it != nextLifetimes.end();)
                if (active.count(it->first) == 0) it = nextLifetimes.erase(it);
                else ++it;
            lifetimes_ = std::move(nextLifetimes);
            nextGeneration_ = nextGeneration;
            snapshot = std::move(next);
            context = std::move(nextContext);
            lastRevision_ = revision;
            published_ = true;
            error.clear();
            return true;
        }
    private:
        static bool IsNativeAttribute(const std::string& id)
        {
            return id == "health.current" || id == "health.maximum" || id == "alive" ||
                id == "ammo.available" || id == "ammo.loaded" || id == "ammo.capacity" || id == "ammo.reserve" ||
                id == "ammo.reloading" || id == "equipment.equipped" || id=="equipment.name" || id=="equipment.slots" || id=="equipment.slot";
        }
        struct Lifetime
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::uint64_t scene = 0;
            std::uint64_t generation = 0;
        };
        std::map<bridge::ScreenBindingContext::Target,Lifetime> lifetimes_;
        std::uint64_t session_ = 0, lastSessionIdentity_ = 0, lastRevision_ = 0, nextGeneration_ = 0;
        bool published_ = false;
    };
}
