#include "renegade/bridge/WorldPickupController.h"
#include "renegade/bridge/IdentityService.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include <sstream>

namespace renegade::bridge {
namespace {
    float DistanceSquared(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        const float x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
        return x*x+y*y+z*z;
    }
}
void WorldPickupController::Reset() noexcept {
    entries_.clear();
    touchLatch_.clear();
    authoredInstanceIds_.clear();
    retiredInstanceIds_.clear();
    dropped_.clear();
    saveIdentityValid_=true;
}
void WorldPickupController::OpenForScene(const wi::scene::Scene& scene,
        const InventoryService& inventory, std::vector<std::string>& diagnostics) {
    Reset();
    diagnostics.clear();
    for (std::size_t i=0; i<scene.metadatas.GetCount(); ++i) {
        const auto entity=scene.metadatas.GetEntity(i);
        const auto* metadata=scene.metadatas.GetComponent(entity);
        if (!metadata || !metadata->bool_values.has(PickupEnabledKey) ||
            !metadata->bool_values.get(PickupEnabledKey)) continue;
        InventoryPickupConfig config;
        std::string error;
        if (!ReadInventoryPickupConfig(scene,entity,config,error)) {
            diagnostics.push_back("World pickup has invalid settings: "+error);
            continue;
        }
        if (!inventory.FindDefinition(config.itemId)) {
            diagnostics.push_back("World pickup references an unavailable item: "+config.itemId);
            continue;
        }
        if (!scene.transforms.Contains(entity)) {
            diagnostics.push_back("World pickup has no transform: "+config.itemId);
            continue;
        }
        const auto* definition=inventory.FindDefinition(config.itemId);
        const auto persistentId=PersistentEntityId(scene,entity);
        if (persistentId.empty()) {
            // Old authored scenes still collect, but cannot yet be saved
            // safely. Re-applying pickup settings in Studio assigns an ID.
            saveIdentityValid_=false;
        } else if (!IsValidStableId(persistentId) ||
                   !authoredInstanceIds_.emplace(persistentId,entity).second) {
            // Do not guess between duplicated prefab/scene identities.
            saveIdentityValid_=false;
            diagnostics.push_back("World pickup has a duplicate or invalid persistent ID.");
            continue;
        }
        entries_.push_back({entity,std::move(config),definition->name,persistentId});
    }
    std::sort(entries_.begin(),entries_.end(),[](const Entry& a,const Entry& b) {
        return a.entity<b.entity;
    });
}
WorldPickupFrame WorldPickupController::Update(wi::scene::Scene& scene,
        const DirectX::XMFLOAT3& position, bool interactPressed, bool inputAllowed,
        InventoryGameplayGateway& playerGateway) {
    WorldPickupFrame output;
    if (!inputAllowed) return output;
    // Select one deterministic nearby pickup per frame. Touch has priority
    // when physically contacted; E chooses the nearest eligible interact item.
    std::size_t chosen=entries_.size();
    float best=std::numeric_limits<float>::infinity();
    bool touch=false;
    for (std::size_t i=0;i<entries_.size();++i) {
        const auto& entry=entries_[i];
        const auto* transform=scene.transforms.GetComponent(entry.entity);
        if (!transform) continue;
        const float distance=DistanceSquared(position,transform->GetPosition());
        const bool contact=distance<=1.25f*1.25f;
        if (!contact) touchLatch_.erase(entry.entity);
        if (entry.config.method==InventoryPickupMethod::OnTouch) {
            if (!contact || touchLatch_.count(entry.entity)) continue;
            if (!touch || distance<best || (distance==best &&
                entry.entity<entries_[chosen].entity)) {
                chosen=i; best=distance; touch=true;
            }
        } else if (!touch && distance<=entry.config.interactDistance*
                     entry.config.interactDistance &&
                   (distance<best || (distance==best &&
                    (chosen==entries_.size() || entry.entity<entries_[chosen].entity)))) {
            chosen=i; best=distance;
        }
    }
    if (chosen==entries_.size()) return output;
    const auto picked=entries_[chosen];
    if (!touch) {
        output.prompt="Press E to collect "+picked.displayName;
        if (!interactPressed) return output;
        output.consumedInteract=true;
    } else {
        touchLatch_.insert(picked.entity);
    }
    output.result=playerGateway.Add("player",picked.config.itemId,picked.config.quantity);
    if (output.result!=InventoryResult::Success) {
        output.prompt=output.result==InventoryResult::CapacityExceeded
            ? "Inventory full - cannot collect "+picked.displayName
            : "Cannot collect "+picked.displayName;
        return output;
    }
    output.collected=true;
    output.collectedEntity=picked.entity;
    output.prompt="Collected "+std::to_string(picked.config.quantity)+
                  " x "+picked.displayName;
    if (picked.config.removeWhenCollected) {
        if(picked.dynamic)
            dropped_.erase(picked.persistentId);
        else if (!picked.persistentId.empty())
            retiredInstanceIds_.insert(picked.persistentId);
        // Consume at most once even if native scene removal is deferred.
        entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(chosen));
        touchLatch_.erase(picked.entity);
        scene.Entity_Remove(picked.entity,true);
    }
    return output;
}

InventoryResult WorldPickupController::DropFromPlayer(
    wi::scene::Scene& scene,const InventoryService& inventory,
    InventoryGameplayGateway& gateway,const std::string& instanceId,
    std::uint32_t quantity,const DirectX::XMFLOAT3& position,std::string& error) {
    if(quantity==0 || instanceId.empty() || dropped_.size()>=1024 ||
       !std::isfinite(position.x)||!std::isfinite(position.y)||
       !std::isfinite(position.z)) {
        error="Invalid world drop, position, or maximum drop count reached.";
        return InventoryResult::InvalidArgument;
    }
    const auto* player=inventory.FindContainer("player");
    if(!player) {error="Player backpack not found.";return InventoryResult::NotFound;}
    const auto stack=std::find_if(player->stacks.begin(),player->stacks.end(),
        [&](const InventoryItemStack& s){return s.instanceId==instanceId;});
    if(stack==player->stacks.end() || stack->quantity<quantity) {
        error="Item quantity changed before drop.";
        return InventoryResult::InsufficientQuantity;
    }
    const auto* definition=inventory.FindDefinition(stack->definitionId);
    if(!definition) {error="Item definition unavailable.";return InventoryResult::NotFound;}
    const std::string itemId=stack->definitionId;
    const auto id=GenerateStableId();
    if(!IsValidStableId(id)) {error="Could not allocate drop identity.";return InventoryResult::InvalidArgument;}
    // A visible, collectible mesh, never a silent inventory-only deletion.
    // The neutral orb is used until the creator supplies a world pickup prefab.
    const auto entity=scene.Entity_CreateSphere("Dropped "+definition->name,0.22f,12,12);
    auto* transform=scene.transforms.GetComponent(entity);
    if(entity==wi::ecs::INVALID_ENTITY || !transform) {
        if(entity!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(entity,true);
        error="World pickup visual could not be created; inventory preserved.";
        return InventoryResult::InvalidArgument;
    }
    transform->ClearTransform();
    transform->Translate(position);
    transform->UpdateTransform();
    if(auto* material=scene.materials.GetComponent(entity)) {
        material->baseColor={.18f,.78f,.59f,1.0f};
        material->emissiveColor={.025f,.13f,.10f,1.0f};
        material->SetDirty();
    }
    InventoryPickupConfig config;
    config.canCollect=true;
    config.itemId=itemId;
    config.quantity=quantity;
    config.method=InventoryPickupMethod::PressE;
    config.removeWhenCollected=true;
    config.interactDistance=2.5f;
    // Register the world object before withdrawing from the player.
    // Every failure path can then safely roll back the visual and registry.
    entries_.push_back({entity,config,definition->name,id,true});
    dropped_.emplace(id,DroppedItem{id,config.itemId,quantity,position,entity});
    const auto removed=gateway.Remove("player",instanceId,quantity);
    if(removed!=InventoryResult::Success) {
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),
            [&](const Entry& e){return e.persistentId==id;}),entries_.end());
        dropped_.erase(id);
        scene.Entity_Remove(entity,true);
        error="Drop cancelled: backpack refused the withdrawal.";
        return removed;
    }
    std::sort(entries_.begin(),entries_.end(),[](const Entry& a,const Entry& b) {
        return a.entity<b.entity;
    });
    error.clear();
    return InventoryResult::Success;
}

bool WorldPickupController::CaptureRuntimeState(
    const std::string& projectId,const std::string& sceneId,
    std::string& out,std::string& error) const {
    out.clear();
    if (!IsValidStableId(projectId) || !IsValidStableId(sceneId)) {
        error="World pickup save requires valid project and scene identities.";
        return false;
    }
    if (!saveIdentityValid_ || authoredInstanceIds_.size()>10000) {
        error="World pickups lack unique persistent IDs. Re-apply the Pickup / Item settings in Studio before saving.";
        return false;
    }
    std::ostringstream stream;
    stream << "RENEGADE_WORLD_PICKUPS 2 " << projectId << ' '
           << sceneId << ' ' << retiredInstanceIds_.size() << ' '
           << dropped_.size() << '\n';
    stream<<std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const auto& id:retiredInstanceIds_)stream << id << '\n';
    for (const auto& [id,drop]:dropped_)
        stream<<id<<' '<<drop.itemId<<' '<<drop.quantity<<' '
              <<drop.position.x<<' '<<drop.position.y<<' '<<drop.position.z<<'\n';
    out=stream.str();
    if (out.size()>4u*1024u*1024u) {
        error="World pickup state exceeds its save budget.";
        out.clear();
        return false;
    }
    error.clear();
    return true;
}
bool WorldPickupController::RestoreRuntimeState(
    wi::scene::Scene& scene,const std::string& projectId,
    const std::string& sceneId,const std::string& bytes,
    std::string& error,bool validateOnly,const InventoryService* inventory) {
    if (!IsValidStableId(projectId) || !IsValidStableId(sceneId) ||
        !saveIdentityValid_ || bytes.empty() ||
        bytes.size()>4u*1024u*1024u) {
        error="World pickup save identity or payload is invalid.";
        return false;
    }
    std::istringstream input(bytes);
    std::string magic,savedProject,savedScene;
    unsigned version=0;
    std::size_t count=0,dropCount=0;
    if (!(input >> magic >> version >> savedProject >> savedScene >> count) ||
        magic!="RENEGADE_WORLD_PICKUPS" || (version!=1 && version!=2) ||
        savedProject!=projectId || savedScene!=sceneId ||
        count>10000 || count>authoredInstanceIds_.size()) {
        error="World pickup save belongs to another project/scene or has an unsupported schema.";
        return false;
    }
    if(version==2 && (!(input>>dropCount) || dropCount>1024)) {
        error="World pickup save belongs to another project/scene or has an unsupported schema.";
        return false;
    }
    std::set<std::string> desired;
    for(std::size_t i=0;i<count;++i) {
        std::string id;
        if (!(input >> id) || !IsValidStableId(id) ||
            !desired.insert(id).second || !authoredInstanceIds_.count(id)) {
            error="World pickup save contains duplicate, unknown or invalid object IDs.";
            return false;
        }
    }
    std::map<std::string,DroppedItem> desiredDrops;
    for(std::size_t i=0;i<dropCount;++i) {
        DroppedItem drop;
        if(!(input>>drop.id>>drop.itemId>>drop.quantity>>
                    drop.position.x>>drop.position.y>>drop.position.z) ||
           !IsValidStableId(drop.id) || desired.count(drop.id) ||
           authoredInstanceIds_.count(drop.id) ||
           !desiredDrops.emplace(drop.id,drop).second ||
           drop.quantity==0 || drop.quantity>100000 ||
           !std::isfinite(drop.position.x) ||
           !std::isfinite(drop.position.y) ||
           !std::isfinite(drop.position.z) ||
           std::fabs(drop.position.x)>10000000.f ||
           std::fabs(drop.position.y)>10000000.f ||
           std::fabs(drop.position.z)>10000000.f ||
           !inventory || !inventory->FindDefinition(drop.itemId)) {
            error="Saved dropped item has invalid ID, quantity, location or item definition.";
            return false;
        }
    }
    std::string extra;
    if (input >> extra) {
        error="World pickup save has unexpected extra data.";
        return false;
    }
    // A retired native scene object cannot be resurrected in-place. Host must
    // reload a fresh authored scene before restoring an older slot.
    for (const auto& id:retiredInstanceIds_)
        if (!desired.count(id)) {
            error="Reload the original scene before restoring an older pickup save.";
            return false;
        }
    for (const auto& id:desired) {
        if (retiredInstanceIds_.count(id)) continue;
        const auto entity=authoredInstanceIds_.at(id);
        if (const auto entry=std::find_if(entries_.begin(),entries_.end(),
                [&](const Entry& e){return e.entity==entity;});
            entry==entries_.end() || !entry->config.removeWhenCollected ||
            !scene.transforms.Contains(entity)) {
            error="Saved world pickup is no longer a removable object in this scene.";
            return false;
        }
    }
    // A preflight is mandatory before replacing the current Runtime level:
    // reject corrupt/mismatched world IDs without altering live gameplay.
    if (validateOnly) { error.clear(); return true; }
    // Commit only after validating the complete payload against this scene.
    entries_.erase(std::remove_if(entries_.begin(),entries_.end(),
        [&](const Entry& e) {
            if (!desired.count(e.persistentId)) return false;
            touchLatch_.erase(e.entity);
            scene.Entity_Remove(e.entity,true);
            return true;
        }),entries_.end());
    // Rebuild persisted dynamic objects only from the fresh authored scene.
    // The validateOnly pass above cannot create or remove scene entities.
    if(!dropped_.empty()) {
        error="Reload the original scene before restoring saved dropped objects.";
        return false;
    }
    for(auto& [id,drop]:desiredDrops) {
        const auto entity=scene.Entity_CreateSphere("Dropped "+
            inventory->FindDefinition(drop.itemId)->name,0.22f,12,12);
        auto* transform=scene.transforms.GetComponent(entity);
        if(entity==wi::ecs::INVALID_ENTITY || !transform) {
            if(entity!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(entity,true);
            for(auto& [cleanupId,created]:dropped_)scene.Entity_Remove(created.entity,true);
            entries_.erase(std::remove_if(entries_.begin(),entries_.end(),
                [](const Entry& e){return e.dynamic;}),entries_.end());
            dropped_.clear();
            error="Failed restoring dropped world mesh; no inventory items were removed.";
            return false;
        }
        transform->ClearTransform();
        transform->Translate(drop.position);
        transform->UpdateTransform();
        if(auto* material=scene.materials.GetComponent(entity)) {
            material->baseColor={.18f,.78f,.59f,1.0f};
            material->emissiveColor={.025f,.13f,.10f,1.0f};
            material->SetDirty();
        }
        drop.entity=entity;
        InventoryPickupConfig config;
        config.canCollect=true;
        config.itemId=drop.itemId;
        config.quantity=drop.quantity;
        config.method=InventoryPickupMethod::PressE;
        config.removeWhenCollected=true;
        config.interactDistance=2.5f;
        entries_.push_back({entity,config,inventory->FindDefinition(drop.itemId)->name,id,true});
        dropped_.emplace(id,drop);
    }
    std::sort(entries_.begin(),entries_.end(),[](const Entry& a,const Entry& b){
        return a.entity<b.entity;
    });
    retiredInstanceIds_=std::move(desired);
    error.clear();
    return true;
}
}
