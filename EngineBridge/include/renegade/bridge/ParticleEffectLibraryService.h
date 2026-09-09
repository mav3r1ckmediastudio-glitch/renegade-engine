#pragma once

#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ParticleEffectService.h"

namespace renegade::bridge
{
    inline constexpr const char* ParticleEffectLibraryFormat =
        "renegade-particle-effect";
    inline constexpr std::uint32_t ParticleEffectLibrarySchemaVersion = 1;
    inline constexpr const char* ParticleEffectManifestName =
        "ParticleEffect.json";
    inline constexpr const char* ParticleEffectPackageExtension = ".rvfx";

    struct ParticleEffectPresetLayer
    {
        std::string name;
        ParticleEmitterState emitter;
        TransformState localTransform;
        wi::enums::BLENDMODE blendMode = wi::enums::BLENDMODE_ALPHA;
        std::string textureLibraryRelativePath;
    };

    struct ParticleEffectPreset
    {
        std::string name;
        std::vector<ParticleEffectPresetLayer> layers;
    };

    struct ParticleEffectLibraryEntry
    {
        std::string name;
        std::string packagePath;
        std::size_t layerCount = 0;
    };

    struct SaveParticleEffectLibraryResult
    {
        bool succeeded = false;
        std::string packagePath;
        std::size_t layerCount = 0;
        std::size_t bundledTextureCount = 0;
        std::string error;
    };

    // Captures either a Renegade Particle Effect root or a standalone native
    // Wicked particle emitter. A standalone emitter becomes a one-layer effect.
    // Governed base-colour/spritesheet textures are bundled into the package so
    // the preset remains reusable across projects rather than retaining a
    // project-specific stable asset ID.
    [[nodiscard]] SaveParticleEffectLibraryResult SaveParticleEffectToLibrary(
        const wi::scene::Scene& scene,
        wi::ecs::Entity effectRootOrEmitter,
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& libraryRoot,
        const std::string& requestedName);

    [[nodiscard]] bool LoadParticleEffectPreset(
        const std::string& packagePath,
        ParticleEffectPreset& preset,
        std::string& error);

    [[nodiscard]] std::vector<ParticleEffectLibraryEntry>
    ListParticleEffectLibrary(
        const std::string& libraryRoot,
        std::string& warning);

    struct PreparedParticleEffectPresetLayer
    {
        ParticleEffectPresetLayer preset;
        bool hasTexture = false;
        PreparedMaterialTextureAsset texture;
    };

    struct PreparedParticleEffectPreset
    {
        std::string name;
        std::vector<PreparedParticleEffectPresetLayer> layers;
    };

    struct PrepareParticleEffectPresetResult
    {
        bool succeeded = false;
        PreparedParticleEffectPreset preset;
        std::size_t importedTextureCount = 0;
        std::string error;
    };

    // Imports any bundled library textures through the existing governed
    // creator texture pipeline, then prepares their stable project products for
    // one atomic scene-placement command. Project imports intentionally persist
    // just like the existing particle Texture/Spritesheet picker; Undo governs
    // the scene instance, not the project's reusable imported dependencies.
    [[nodiscard]] PrepareParticleEffectPresetResult PrepareParticleEffectPresetForProject(
        const std::string& packagePath,
        const std::string& projectRoot,
        const StableId& projectId);

    class CreateParticleEffectFromPresetCommand final : public ICommand
    {
    public:
        CreateParticleEffectFromPresetCommand(
            wi::scene::Scene& scene,
            const XMFLOAT3& position,
            PreparedParticleEffectPreset preset);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity RootEntity() const noexcept
        {
            return root_;
        }
        [[nodiscard]] const std::vector<wi::ecs::Entity>& LayerEntities() const noexcept
        {
            return layerEntities_;
        }
        [[nodiscard]] const std::string& Error() const noexcept
        {
            return error_;
        }

    private:
        bool CreateFirstTime();
        bool RestoreSnapshot();
        bool ApplyPreparedTextures();

        wi::scene::Scene* scene_ = nullptr;
        XMFLOAT3 position_ = {};
        PreparedParticleEffectPreset preset_;
        wi::ecs::Entity root_ = wi::ecs::INVALID_ENTITY;
        std::vector<wi::ecs::Entity> layerEntities_;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
        std::string error_;
    };
}
