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
        // Opt-in gameplay charge policy. damage is the full-charge impact amount.
        // Zero duration preserves fixed legacy damage.
        float fullChargeSeconds = 0, minimumChargeDamage = 0;
        bool scorchOnImpact = false;
        BurningPayload burning, minimumChargeBurning;
        GrenadePhysicsPolicy grenade; // Explicit opt-in; thrown objects need not explode.
        // Optional registered model appearance, independent of collision.
        StableId meshAssetId;
        float visualScale = 1;
        std::array<float, 3> visualRotationDegrees{0, 0, 0};
        std::vector<ProjectileEffectLayer> flightEffects;
        ProjectileEffectKind impactEffect = ProjectileEffectKind::None;
        bool stickOnImpact = false;
        // Explicit built-in fireball appearance, shared by palm charge and flight.
        bool fireballPresentation = false;
        float stuckLifetimeSeconds = 30, embedDepthMetres = .05f;
    };
    inline float ProjectileChargedDamage(const ProjectileAssetDocument& d, float seconds) noexcept
    {
        if (d.fullChargeSeconds <= 0) return d.damage;
        const float q = std::isfinite(seconds) ? std::clamp(seconds / d.fullChargeSeconds, 0.f, 1.f) : 0.f;
        return d.minimumChargeDamage + (d.damage - d.minimumChargeDamage) * q;
    }
    inline BurningPayload ProjectileChargedBurning(const ProjectileAssetDocument& d, float seconds) noexcept
    {
        const float q=d.fullChargeSeconds<=0 ? 1.f : (std::isfinite(seconds)?std::clamp(seconds/d.fullChargeSeconds,0.f,1.f):0.f);
        return {d.minimumChargeBurning.damagePerSecond+(d.burning.damagePerSecond-d.minimumChargeBurning.damagePerSecond)*q,
            d.minimumChargeBurning.durationSeconds+(d.burning.durationSeconds-d.minimumChargeBurning.durationSeconds)*q};
    }
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
