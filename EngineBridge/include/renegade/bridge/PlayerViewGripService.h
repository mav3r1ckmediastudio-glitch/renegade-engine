#pragma once
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr std::array<const char*, 3> PlayerViewSocketMetadataKeys = {
        "renegade.player.view_socket.primary",
        "renegade.player.view_socket.off_hand",
        "renegade.player.view_socket.two_hand_support",
    };
    struct PlayerViewGripBinding
    {
        // Canonical JSON array of ancestor names; no transient ECS IDs.
        std::string bonePath;
        XMFLOAT3 position = {};
        XMFLOAT3 rotationDegrees = {};
    };
    using PlayerViewGripSettings = std::array<PlayerViewGripBinding, 3>;
    struct PlayerViewBoneChoice
    {
        std::string path;
        std::string label;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
    };

    bool CollectPlayerViewBones(const wi::scene::Scene& scene,
        std::vector<PlayerViewBoneChoice>& bones, std::string& error);
    bool ValidatePlayerViewGrips(const PlayerViewGripSettings& settings,
        const std::vector<PlayerViewBoneChoice>* bones, std::string& error);
    bool CapturePlayerViewGrips(const wi::scene::Scene& scene,
        PlayerViewGripSettings& settings, std::string& error);
    bool ApplyPlayerViewGrips(wi::scene::Scene& scene,
        const PlayerViewGripSettings& settings, std::string& error);
    bool ParsePlayerViewGripOptions(const std::string& json,
        PlayerViewGripSettings& settings, std::string& error);
    bool SerializePlayerViewGripOptions(const PlayerViewGripSettings& settings,
        std::string& json, std::string& error);

    // A private asset working copy. Undo/Redo affect only this session until Save.
    // Save journals the product, managed projection and registry as one transaction.
    class PlayerViewGripSession
    {
    public:
        PlayerViewGripSession();
        ~PlayerViewGripSession();
        PlayerViewGripSession(const PlayerViewGripSession&) = delete;
        PlayerViewGripSession& operator=(const PlayerViewGripSession&) = delete;
        bool Open(const std::string& projectRoot, const std::string& projectId,
            const std::string& assetId, std::string& error);
        const std::vector<PlayerViewBoneChoice>& Bones() const;
        const PlayerViewGripSettings& Settings() const;
        const std::string& AssetPath() const;
        bool SetBinding(std::size_t role, const PlayerViewGripBinding& binding,
            std::string& error);
        bool Undo();
        bool Redo();
        bool CanUndo() const;
        bool CanRedo() const;
        bool IsDirty() const;
        bool Save(std::string& error, ProjectDocumentTransactionHook hook = {});
    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
