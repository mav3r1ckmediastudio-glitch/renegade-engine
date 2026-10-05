#pragma once
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <vector>
#include <memory>

namespace renegade::bridge
{
    inline constexpr const char* PlayerPrefabExtension = ".rplayerprefab";
    inline constexpr const char* PlayerPrefabOriginKey = "renegade.player.prefab_asset_id";
    inline constexpr const char* PlayerPrefabBaselineKey = "renegade.player.prefab_baseline";
    struct PlayerPrefabDocument
    {
        StableId projectId, assetId;
        std::string name;
        PlayerControllerSettings settings;
    };
    struct PlayerPrefabSaveResult
    {
        bool succeeded = false;
        PlayerPrefabDocument document;
        std::string projectRelativePath, error;
        ProjectDocumentTransactionResult transaction;
    };
    bool SerializePlayerPrefab(const PlayerPrefabDocument&, std::string&, std::string&);
    bool DeserializePlayerPrefab(const std::string&, PlayerPrefabDocument&, std::string&);
    bool ReadPlayerPrefabFile(const std::string&, PlayerPrefabDocument&, std::string&);
    bool LoadPlayerPrefab(const std::string& projectRoot, const StableId& projectId,
        const StableId& assetId, PlayerPrefabDocument&, std::string&);
    PlayerPrefabSaveResult SavePlayerPrefab(const std::string& projectRoot,
        const StableId& projectId, const std::string& name,
        const PlayerControllerSettings&, ProjectDocumentTransactionHook hook = {});
    std::vector<PlayerPrefabDocument> ListPlayerPrefabs(const std::string& projectRoot,
        const StableId& projectId, std::string& error);
    bool EnsureBasicPlayerPrefab(const std::string& root, const StableId& project, std::string& error);
    class ApplyPlayerPrefabCommand;
    class PlacePlayerPrefabCommand final : public ICommand
    {
    public:
        PlacePlayerPrefabCommand(wi::scene::Scene&, TransformState, PlayerPrefabDocument);
        bool Execute() override;
        void Undo() override;
        wi::ecs::Entity PlacedEntity() const noexcept;
    private:
        wi::scene::Scene* scene_;
        CreatePlayerStartCommand create_;
        PlayerPrefabDocument document_;
        std::unique_ptr<ApplyPlayerPrefabCommand> apply_;
    };
    StableId CapturePlayerPrefabOrigin(const wi::scene::Scene&, wi::ecs::Entity);
    bool CapturePlayerPrefabBaseline(const wi::scene::Scene&, wi::ecs::Entity,
        PlayerPrefabDocument&, std::string&);
    bool PlayerSettingsEqual(const PlayerControllerSettings&, const PlayerControllerSettings&);
    // Applies a resolved copy. Level-local edits are explicit overrides; the
    // persisted baseline makes Reset deterministic without external Runtime IO.
    class ApplyPlayerPrefabCommand final : public ICommand
    {
    public:
        ApplyPlayerPrefabCommand(wi::scene::Scene&, wi::ecs::Entity, PlayerPrefabDocument);
        bool Execute() override;
        void Undo() override;
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        PlayerPrefabDocument document_;
        std::unique_ptr<SetPlayerControllerSettingsCommand> settings_;
        std::string beforeOrigin_, beforeBaseline_, afterBaseline_;
        bool captured_ = false;
    };
}
