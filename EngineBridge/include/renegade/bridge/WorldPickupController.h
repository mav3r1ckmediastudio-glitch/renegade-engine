#pragma once
// G5: bounded world pickup controller. Runtime-only state; the authored scene
// metadata and authoritative InventoryService remain the sources of truth.
#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/InventoryGameplayGateway.h"
#include <DirectXMath.h>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace renegade::bridge {
struct WorldPickupFrame {
    std::string prompt;
    bool consumedInteract = false;
    bool collected = false;
    wi::ecs::Entity collectedEntity = wi::ecs::INVALID_ENTITY;
    InventoryResult result = InventoryResult::Success;
};
class WorldPickupController {
public:
    struct Entry {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        InventoryPickupConfig config;
        std::string displayName;
        std::string persistentId;
        bool dynamic=false;
    };
    struct DroppedItem {
        std::string id, itemId;
        std::uint32_t quantity=0;
        DirectX::XMFLOAT3 position{};
        wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;
    };
    // Trusted native Runtime drop. A real scene entity is created before the
    // scoped withdrawal; failed withdrawal destroys that entity immediately.
    InventoryResult DropFromPlayer(wi::scene::Scene& scene,
        const InventoryService& inventory,InventoryGameplayGateway& gateway,
        const std::string& instanceId,std::uint32_t quantity,
        const DirectX::XMFLOAT3& position,std::string& error);
    [[nodiscard]] std::size_t DroppedCount() const noexcept {return dropped_.size();}
    void Reset() noexcept;
    // Invalid saved entries are skipped with diagnostics, never auto-enabled.
    void OpenForScene(const wi::scene::Scene& scene, const InventoryService& inventory,
                      std::vector<std::string>& diagnostics);
    [[nodiscard]] std::size_t CandidateCount() const noexcept { return entries_.size(); }
    // A versioned *payload* for the future governed Save Game owner. The host
    // must atomically pair it with InventorySession state, project and scene
    // identity, and restore into a fresh authored scene before gameplay.
    // These methods NEVER write to the creator's .wiscene or .rinventory.
    [[nodiscard]] bool CaptureRuntimeState(const std::string& projectId,
        const std::string& sceneId,std::string& out,std::string& error) const;
    [[nodiscard]] bool RestoreRuntimeState(wi::scene::Scene& scene,
        const std::string& projectId,const std::string& sceneId,
        const std::string& bytes,std::string& error,bool validateOnly=false,
        const InventoryService* inventory=nullptr);
    // Input and physics are supplied by the host. No unmanaged file access,
    // raw keyboard polling, script-grant modification or extra inventory owner.
    WorldPickupFrame Update(wi::scene::Scene& scene, const DirectX::XMFLOAT3& playerPosition,
                            bool interactPressed, bool inputAllowed,
                            InventoryGameplayGateway& playerGateway);
private:
    std::vector<Entry> entries_;
    std::set<wi::ecs::Entity> touchLatch_;
    std::map<std::string,wi::ecs::Entity> authoredInstanceIds_;
    std::set<std::string> retiredInstanceIds_;
    std::map<std::string,DroppedItem> dropped_;
    bool saveIdentityValid_=true;
};
} // namespace renegade::bridge
