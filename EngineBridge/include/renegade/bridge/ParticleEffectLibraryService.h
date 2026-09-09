#pragma once

#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/IdentityService.h"
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
}
