#pragma once

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* CharacterPrefabFormat =
        "renegade-character-prefab";
    inline constexpr const char* CharacterPrefabExtension = ".rcharprefab";
    inline constexpr std::uint32_t CharacterPrefabSchemaVersion = 1;

    inline constexpr const char* CharacterPrefabOriginAssetMetadataKey =
        "renegade.character_prefab.origin_asset_id";
    inline constexpr const char* CharacterPrefabBaseAssetMetadataKey =
        "renegade.character_prefab.base_character_asset_id";
    inline constexpr const char* CharacterPrefabVersionMetadataKey =
        "renegade.character_prefab.version";

    struct CharacterPrefabScriptProperty
    {
        ScriptPropertyValue value;
        bool selfEntityReference = false;
    };

    struct CharacterPrefabScriptTemplate
    {
        ScriptSourceBinding source;
        bool enabled = true;
        std::uint32_t order = 0;
        std::vector<CharacterPrefabScriptProperty> properties;
    };

    struct CharacterPrefabDocument
    {
        std::string formatIdentifier = CharacterPrefabFormat;
        std::uint32_t schemaVersion = CharacterPrefabSchemaVersion;
        StableId projectId;
        StableId assetId;
        StableId baseCharacterAssetId;
        CharacterAuthoringSettings settings;
        std::string advancedOverrides;
        std::vector<CharacterPrefabScriptTemplate> scripts;
    };

    struct CharacterPrefabSaveRequest
    {
        std::string projectRoot;
        StableId projectId;
        wi::scene::Scene* scene = nullptr;
        wi::ecs::Entity character = wi::ecs::INVALID_ENTITY;
        std::string prefabName;
        const ScriptDocument* scriptDocument = nullptr;
        ProjectDocumentTransactionHook transactionHook;
    };

    struct CharacterPrefabSaveResult
    {
        bool succeeded = false;
        StableId assetId;
        StableId baseCharacterAssetId;
        std::string projectRelativePath;
        std::vector<std::string> warnings;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    struct PreparedCharacterPrefabPlacement
    {
        bool succeeded = false;
        CharacterPrefabDocument document;
        wi::allocator::shared_ptr<wi::scene::Scene> scene;
        ModelBounds bounds;
        float scale = 1.0f;
        std::string error;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return succeeded && error.empty() && scene.IsValid();
        }

        [[nodiscard]] wi::scene::Scene* PeekMutableScene() noexcept
        {
            return scene.IsValid() ? scene.get() : nullptr;
        }

        [[nodiscard]] const wi::scene::Scene* PeekScene() const noexcept
        {
            return scene.IsValid() ? scene.get() : nullptr;
        }

        [[nodiscard]] wi::allocator::shared_ptr<wi::scene::Scene>
        ReleaseScene() noexcept
        {
            return std::move(scene);
        }
    };

    [[nodiscard]] bool SerializeCharacterPrefab(
        const CharacterPrefabDocument& document,
        std::string& json,
        std::string& error);

    [[nodiscard]] bool DeserializeCharacterPrefab(
        const std::string& json,
        CharacterPrefabDocument& document,
        std::string& error);

    [[nodiscard]] bool IsCharacterPrefabProjectPath(
        const std::string& projectRelativePath) noexcept;

    [[nodiscard]] CharacterPrefabSaveResult SaveCharacterPrefab(
        const CharacterPrefabSaveRequest& request);

    [[nodiscard]] PreparedCharacterPrefabPlacement PrepareCharacterPrefabPlacement(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& prefabAssetId);

    // Character duplication is deliberately stronger than raw Wicked entity
    // duplication. It preserves authored Character/advanced/script setup while
    // rebuilding the native Character controller and assigning fresh persistent
    // entity/script identities. Runtime cognition remains transient and is never
    // copied into this authoring command.
    class DuplicateCharacterInstanceCommand final : public ICommand
    {
    public:
        DuplicateCharacterInstanceCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity source,
            ScriptDocument* scriptDocument = nullptr);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity DuplicatedEntity() const noexcept;

    private:
        [[nodiscard]] bool ApplyCharacterAuthoring();
        [[nodiscard]] bool ApplyScriptDuplication();

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity source_ = wi::ecs::INVALID_ENTITY;
        std::unique_ptr<DuplicateEntityCommand> duplicateCommand_;
        wi::ecs::Entity duplicate_ = wi::ecs::INVALID_ENTITY;
        CharacterAuthoringSettings settings_;
        CharacterAdvancedOverrides advancedOverrides_;
        ScriptDocument* scriptDocument_ = nullptr;
        ScriptDocument scriptBefore_;
        ScriptDocument scriptAfter_;
        bool scriptChanged_ = false;
        bool captured_ = false;
    };

    // Places the base prepared Character payload while applying a portable
    // prefab authoring layer. The underlying reusable instance continues to
    // reference the base Character Asset; prefab origin is stamped separately.
    class PlaceCharacterPrefabCommand final : public ICommand
    {
    public:
        PlaceCharacterPrefabCommand(
            wi::scene::Scene& scene,
            wi::allocator::shared_ptr<wi::scene::Scene> preparedBaseCharacter,
            CharacterPrefabDocument document,
            const XMFLOAT3& placementPosition,
            float scaleFactor,
            std::string displayName = {},
            ScriptDocument* scriptDocument = nullptr);

        PlaceCharacterPrefabCommand(
            wi::scene::Scene& scene,
            CharacterPrefabDocument document,
            wi::ecs::Entity existingInstanceRoot,
            wi::ecs::Entity existingPayloadRoot,
            std::size_t firstMaterialIndex,
            std::string displayName = {},
            ScriptDocument* scriptDocument = nullptr);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity PlacedEntity() const noexcept;

    private:
        [[nodiscard]] bool ApplyPrefabLayer();
        [[nodiscard]] bool InstantiateScripts();

        wi::scene::Scene* scene_ = nullptr;
        CharacterPrefabDocument document_;
        std::unique_ptr<PlaceReusableModelCommand> placementCommand_;
        wi::ecs::Entity placed_ = wi::ecs::INVALID_ENTITY;
        ScriptDocument* scriptDocument_ = nullptr;
        ScriptDocument scriptBefore_;
        ScriptDocument scriptAfter_;
        bool scriptChanged_ = false;
        bool captured_ = false;
    };
}
