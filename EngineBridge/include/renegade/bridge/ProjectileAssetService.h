#pragma once
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectileSimulation.h"
#include <array>

namespace renegade::bridge
{
    inline constexpr const char* ProjectileAssetExtension = ".rprojectile";
    enum class ProjectilePreset { Bullet, Arrow, Bolt, Thrown, Spell };
    enum class ProjectileEffectKind { None, Flame, Smoke, Sparks, Tracer };
    struct ProjectileEffectLayer {
        ProjectileEffectKind kind = ProjectileEffectKind::None;
        std::array<float,3> offset{0,0,0};
        float sizeMetres = .08f, particlesPerSecond = 60, particleLifeSeconds = .3f;
    };
    struct ProjectileAssetDocument
    {
        StableId projectId, assetId;
        std::string name;
        float speedMetresPerSecond = 300;
        float gravityScale = 0;
        float lifetimeSeconds = 5;
        // Payload hook; no complete Player/NPC health feature implied.
        float damage = 10;
        // Optional registered model appearance, independent of collision.
        StableId meshAssetId;
        float visualScale = 1;
        std::array<float, 3> visualRotationDegrees{0, 0, 0};
        std::vector<ProjectileEffectLayer> flightEffects;
        ProjectileEffectKind impactEffect = ProjectileEffectKind::None;
        bool stickOnImpact = false;
        float stuckLifetimeSeconds = 30, embedDepthMetres = .05f;
    };
    struct ProjectileAssetSaveResult
    {
        bool succeeded = false;
        ProjectileAssetDocument document;
        std::string projectRelativePath, error;
        ProjectDocumentTransactionResult transaction;
    };
    const char* ProjectilePresetName(ProjectilePreset preset) noexcept;
    ProjectileAssetDocument MakeProjectilePreset(ProjectilePreset preset);
    bool ValidateProjectileAsset(const ProjectileAssetDocument&, std::string&);
    bool SerializeProjectileAsset(const ProjectileAssetDocument&, std::string&, std::string&);
    bool DeserializeProjectileAsset(const std::string&, ProjectileAssetDocument&, std::string&);
    bool ReadProjectileAssetFile(const std::string&, ProjectileAssetDocument&, std::string&);
    bool LoadProjectileAsset(const std::string& root, const StableId& project, const StableId& id,
                             ProjectileAssetDocument&, std::string&);
    ProjectileAssetSaveResult SaveProjectileAsset(const std::string& root, const StableId& project,
        ProjectileAssetDocument, ProjectDocumentTransactionHook hook = {});
    std::vector<ProjectileAssetDocument> ListProjectileAssets(
        const std::string& root, const StableId& project, std::string&);
    ProjectileDefinition ProjectileSimulationDefinition(const ProjectileAssetDocument&) noexcept;
}
