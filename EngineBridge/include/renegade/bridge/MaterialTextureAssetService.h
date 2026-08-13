#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr int MaterialTextureAssetBindingVersion = 1;
    inline constexpr const char* MaterialTextureAssetBindingVersionMetadataKey =
        "renegade.material_texture_binding_version";
    inline constexpr const char* MaterialBaseColorTextureAssetIdMetadataKey =
        "renegade.material_texture.base_color.asset_id";

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

    struct MaterialTextureBindingRecord
    {
        wi::ecs::Entity materialEntity = wi::ecs::INVALID_ENTITY;
        StableId baseColorTextureAssetId;
    };

    struct MaterialTextureRestoreResult
    {
        bool succeeded = false;
        std::size_t discovered = 0;
        std::size_t restored = 0;
        std::string error;
    };

    // Resolve one governed texture by stable LC01 product ID and prepare the
    // exact retained payload for Wicked. This never mutates a scene.
    [[nodiscard]] bool PrepareMaterialTextureAsset(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        PreparedMaterialTextureAsset& prepared,
        std::string& error);

    // Production loader: decode the governed payload directly from memory via
    // the pinned Wicked resource manager. No extracted source copy is required.
    [[nodiscard]] wi::Resource LoadPreparedMaterialTextureAsset(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error);

    // Persistent WISCENE reference inspection. Stable IDs live in Wicked's
    // serializable MetadataComponent on the material entity; TextureMap::name
    // remains empty so Wicked never treats a .rasset as an ordinary image.
    [[nodiscard]] bool InspectMaterialTextureBindings(
        const wi::scene::Scene& scene,
        std::vector<MaterialTextureBindingRecord>& bindings,
        std::string& error);

    // Rehydrates serialized stable-ID bindings after WISCENE load. Already-live
    // resources are skipped, so calling this from Studio Update is idempotent.
    [[nodiscard]] MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader = {});

    // Gate 4 transient refresh after a governed texture product is reimported.
    // Durable WISCENE identity is unchanged, so this is deliberately not a
    // creator command: every binding to the stable asset ID is force-reloaded
    // from the newly accepted .rasset payload without manual reassignment.
    [[nodiscard]] MaterialTextureRestoreResult RefreshMaterialTextureBindingsForAsset(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        MaterialTextureResourceLoader loader = {});

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
            return prepared_.assetId;
        }
        [[nodiscard]] const std::string& Error() const noexcept
        {
            return error_;
        }

    private:
        bool ApplyPrepared();
        void CaptureBefore();
        void RestoreBefore() noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity materialEntity_ = wi::ecs::INVALID_ENTITY;
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
}
