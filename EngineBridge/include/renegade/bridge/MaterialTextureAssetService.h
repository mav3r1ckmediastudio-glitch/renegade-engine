#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Version 1 remains backward-compatible: LP08 originally persisted only
    // base colour. Creator Recovery extends the same metadata contract with
    // additional independent governed slots instead of invalidating old scenes.
    inline constexpr int MaterialTextureAssetBindingVersion = 1;
    inline constexpr const char* MaterialTextureAssetBindingVersionMetadataKey =
        "renegade.material_texture_binding_version";
    inline constexpr const char* MaterialBaseColorTextureAssetIdMetadataKey =
        "renegade.material_texture.base_color.asset_id";
    inline constexpr const char* MaterialNormalTextureAssetIdMetadataKey =
        "renegade.material_texture.normal.asset_id";
    inline constexpr const char* MaterialSurfaceTextureAssetIdMetadataKey =
        "renegade.material_texture.surface.asset_id";
    inline constexpr const char* MaterialEmissiveTextureAssetIdMetadataKey =
        "renegade.material_texture.emissive.asset_id";
    inline constexpr const char* MaterialOcclusionTextureAssetIdMetadataKey =
        "renegade.material_texture.occlusion.asset_id";

    enum class MaterialTextureSlot
    {
        BaseColor,
        Normal,
        Surface,
        Emissive,
        Occlusion,
    };

    [[nodiscard]] wi::scene::MaterialComponent::TEXTURESLOT WickedTextureSlot(
        MaterialTextureSlot slot) noexcept;
    [[nodiscard]] const char* MaterialTextureSlotMetadataKey(
        MaterialTextureSlot slot) noexcept;
    [[nodiscard]] const char* MaterialTextureSlotLabel(
        MaterialTextureSlot slot) noexcept;

    struct PreparedMaterialTextureAsset
    {
        StableId projectId;
        StableId assetId;
        std::string productProjectRelativePath;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string payloadHash;
        std::string logicalResourceName;
        std::vector<std::uint8_t> payload;
    };

    using MaterialTextureResourceLoader = std::function<wi::Resource(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error)>;

    // One record represents one material/slot binding. baseColorTextureAssetId
    // is retained as a compatibility mirror for Gate 1-era callers/tests and is
    // populated only when slot == BaseColor; new code must use textureAssetId.
    struct MaterialTextureBindingRecord
    {
        wi::ecs::Entity materialEntity = wi::ecs::INVALID_ENTITY;
        MaterialTextureSlot slot = MaterialTextureSlot::BaseColor;
        StableId textureAssetId;
        StableId baseColorTextureAssetId;
    };

    struct MaterialTextureRestoreResult
    {
        bool succeeded = false;
        std::size_t discovered = 0;
        std::size_t restored = 0;
        std::string error;
    };

    [[nodiscard]] bool PrepareMaterialTextureAsset(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        PreparedMaterialTextureAsset& prepared,
        std::string& error);

    [[nodiscard]] wi::Resource LoadPreparedMaterialTextureAsset(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error);

    // Apply an already-resolved governed resource to one Wicked material slot
    // and persist its stable ID in serializable material metadata.
    [[nodiscard]] bool ApplyPreparedMaterialTextureAsset(
        wi::scene::Scene& scene,
        wi::ecs::Entity materialEntity,
        MaterialTextureSlot slot,
        const PreparedMaterialTextureAsset& prepared,
        MaterialTextureResourceLoader loader,
        std::string& error);

    [[nodiscard]] bool InspectMaterialTextureBindings(
        const wi::scene::Scene& scene,
        std::vector<MaterialTextureBindingRecord>& bindings,
        std::string& error);

    [[nodiscard]] MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader = {});

    [[nodiscard]] MaterialTextureRestoreResult RefreshMaterialTextureBindingsForAsset(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        MaterialTextureResourceLoader loader = {});

    class SetMaterialTextureAssetCommand final : public ICommand
    {
    public:
        SetMaterialTextureAssetCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity materialEntity,
            MaterialTextureSlot slot,
            PreparedMaterialTextureAsset prepared,
            MaterialTextureResourceLoader loader = {});

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const StableId& AssetId() const noexcept
        {
            return prepared_.assetId;
        }
        [[nodiscard]] MaterialTextureSlot Slot() const noexcept
        {
            return slot_;
        }
        [[nodiscard]] const std::string& Error() const noexcept
        {
            return error_;
        }

    private:
        void CaptureBefore();
        void RestoreBefore() noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity materialEntity_ = wi::ecs::INVALID_ENTITY;
        MaterialTextureSlot slot_ = MaterialTextureSlot::BaseColor;
        PreparedMaterialTextureAsset prepared_;
        MaterialTextureResourceLoader loader_;
        wi::scene::MaterialComponent::TextureMap beforeTexture_;
        bool capturedBefore_ = false;
        bool hadMetadata_ = false;
        bool hadVersion_ = false;
        int beforeVersion_ = 0;
        bool hadAssetId_ = false;
        StableId beforeAssetId_;
        std::string error_;
    };

    // Compatibility command retained for existing LP08 Studio/tests. New
    // importer code uses SetMaterialTextureAssetCommand with an explicit slot.
    class SetMaterialBaseColorTextureAssetCommand final : public ICommand
    {
    public:
        SetMaterialBaseColorTextureAssetCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity materialEntity,
            PreparedMaterialTextureAsset prepared,
            MaterialTextureResourceLoader loader = {});

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const StableId& AssetId() const noexcept
        {
            return command_.AssetId();
        }
        [[nodiscard]] const std::string& Error() const noexcept
        {
            return command_.Error();
        }

    private:
        SetMaterialTextureAssetCommand command_;
    };
}
