#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "json.hpp"
#include "renegade/bridge/ReusableAssetService.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iterator>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;
        using json = nlohmann::json;
        const AssetRecord* Find(const AssetRegistry& registry, const StableId& id)
        {
            for (const auto& record : registry.records)
                if (record.assetId == id) return &record;
            return nullptr;
        }
        std::vector<StableId> VisualDependencies(const ProjectileAssetDocument& d)
        {
            return d.meshAssetId.empty() ? std::vector<StableId>{} :
                std::vector<StableId>{d.meshAssetId};
        }
        bool ValidateMesh(const std::string& root, const StableId& project,
                          const AssetRegistry& registry, const ProjectileAssetDocument& d,
                          std::string& error)
        {
            if (d.meshAssetId.empty()) return true;
            const auto* mesh = Find(registry, d.meshAssetId);
            if (!mesh || !mesh->sourceAvailable ||
                mesh->dependencyClass != DependencyClass::ImportedContent ||
                fs::u8path(mesh->projectRelativePath).extension() != ReusableAssetExtension)
            { error = "Projectile mesh must be an available imported model."; return false; }
            const auto path = ResolveDependencyPath(root, mesh->projectRelativePath);
            ReusableModelAssetDocument model;
            if (!path.accepted || !path.exists ||
                !ReadReusableModelAssetDocument(path.absolutePath, model, error)) return false;
            if (model.manifest.projectId != project || model.manifest.assetId != d.meshAssetId)
            { error = "Projectile mesh identity differs from its registry."; return false; }
            return true;
        }
        std::string Hash(const std::string& text)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ull; }
            std::ostringstream out;
            out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
            return out.str();
        }
    }

    const char* ProjectilePresetName(ProjectilePreset preset) noexcept
    {
        switch (preset)
        {
        case ProjectilePreset::Bullet: return "Bullet";
        case ProjectilePreset::Arrow: return "Arrow";
        case ProjectilePreset::Bolt: return "Bolt";
        case ProjectilePreset::Thrown: return "Thrown projectile";
        case ProjectilePreset::Spell: return "Spell projectile";
        default: return "Projectile";
        }
    }

    ProjectileAssetDocument MakeProjectilePreset(ProjectilePreset preset)
    {
        ProjectileAssetDocument result;
        result.name = ProjectilePresetName(preset);
        switch (preset)
        {
        case ProjectilePreset::Bullet: result.speedMetresPerSecond = 300; break;
        case ProjectilePreset::Arrow:
            result.speedMetresPerSecond = 45; result.gravityScale = 1; break;
        case ProjectilePreset::Bolt:
            result.speedMetresPerSecond = 65; result.gravityScale = 1; break;
        case ProjectilePreset::Thrown:
            result.speedMetresPerSecond = 15; result.gravityScale = 1; break;
        case ProjectilePreset::Spell:
            result.speedMetresPerSecond = 20; result.lifetimeSeconds = 8; break;
        }
        return result;
    }

    bool ValidateProjectileAsset(const ProjectileAssetDocument& d, std::string& error)
    {
        bool nameValid = !d.name.empty() && d.name.size() <= 96;
        bool nonSpace = false;
        for (unsigned char c : d.name)
        {
            if (c < 32 || c == 127) nameValid = false;
            if (c != ' ') nonSpace = true;
        }
        if (!IsValidStableId(d.projectId) || !IsValidStableId(d.assetId) ||
            !nameValid || !nonSpace || !std::isfinite(d.speedMetresPerSecond) ||
            d.speedMetresPerSecond <= 0 || d.speedMetresPerSecond > 2000 ||
            !std::isfinite(d.gravityScale) || d.gravityScale < 0 || d.gravityScale > 10 ||
            !std::isfinite(d.lifetimeSeconds) || d.lifetimeSeconds < 0.05f ||
            d.lifetimeSeconds > 120 || !std::isfinite(d.damage) ||
            d.damage < 0 || d.damage > 100000 ||
            (!d.meshAssetId.empty() && !IsValidStableId(d.meshAssetId)) ||
            !std::isfinite(d.visualScale) || d.visualScale < 0.001f || d.visualScale > 100 ||
            std::any_of(d.visualRotationDegrees.begin(), d.visualRotationDegrees.end(),
                [](float v){ return !std::isfinite(v) || std::abs(v) > 360; }))
        {
            error = "Projectile requires a name, valid identity and bounded flight values.";
            return false;
        }
        if(d.flightEffects.size()>2 || unsigned(d.impactEffect)>4 ||
           !std::isfinite(d.stuckLifetimeSeconds)||d.stuckLifetimeSeconds<.1f||d.stuckLifetimeSeconds>120 ||
           !std::isfinite(d.embedDepthMetres)||d.embedDepthMetres<0||d.embedDepthMetres>2 ||
           (d.stickOnImpact && d.meshAssetId.empty())) {
            error="Stick needs a visible model; retained lifetime/depth and effect values must be bounded.";return false;
        }
        for(const auto& effect:d.flightEffects) {
            if(unsigned(effect.kind)==0||unsigned(effect.kind)>4 ||
               !std::isfinite(effect.sizeMetres)||effect.sizeMetres<.005f||effect.sizeMetres>2 ||
               !std::isfinite(effect.particlesPerSecond)||effect.particlesPerSecond<1||effect.particlesPerSecond>500 ||
               !std::isfinite(effect.particleLifeSeconds)||effect.particleLifeSeconds<.02f||effect.particleLifeSeconds>5 ||
               std::any_of(effect.offset.begin(),effect.offset.end(),[](float v){return !std::isfinite(v)||std::abs(v)>10;})) {
                error="Projectile effect layer contains invalid size, rate, lifetime or offset.";return false;
            }
        }
        error.clear();
        return true;
    }

    bool SerializeProjectileAsset(const ProjectileAssetDocument& d, std::string& text, std::string& error)
    {
        if (!ValidateProjectileAsset(d, error)) return false;
        json effects=json::array();
        for(const auto& e:d.flightEffects)effects.push_back({{"kind",unsigned(e.kind)},{"offset",e.offset},
            {"size_metres",e.sizeMetres},{"rate",e.particlesPerSecond},{"life_seconds",e.particleLifeSeconds}});
        text = json{{"format","renegade-projectile"},{"schema_version",3},
            {"project_id",d.projectId},{"asset_id",d.assetId},{"name",d.name},
            {"speed_metres_per_second",d.speedMetresPerSecond},{"gravity_scale",d.gravityScale},
            {"lifetime_seconds",d.lifetimeSeconds},{"damage",d.damage},
            {"mesh_asset_id",d.meshAssetId},{"visual_scale",d.visualScale},
            {"visual_rotation_degrees",d.visualRotationDegrees},{"flight_effects",effects},
            {"impact_effect",unsigned(d.impactEffect)},{"stick_on_impact",d.stickOnImpact},
            {"stuck_lifetime_seconds",d.stuckLifetimeSeconds},{"embed_depth_metres",d.embedDepthMetres}}.dump(2);
        return true;
    }

    bool DeserializeProjectileAsset(const std::string& text, ProjectileAssetDocument& out, std::string& error)
    {
        try
        {
            const auto j = json::parse(text);
            if (!j.is_object() || j.at("format") != "renegade-projectile" ||
                !j.at("schema_version").is_number_integer() ||
                !((j.at("schema_version") == 1 && j.size() == 9) ||
                  (j.at("schema_version") == 2 && j.size() == 12) ||
                  (j.at("schema_version") == 3 && j.size() == 17)))
                throw std::runtime_error("Unsupported projectile schema.");
            for (const char* field : {"speed_metres_per_second","gravity_scale","lifetime_seconds","damage"})
                if (!j.at(field).is_number()) throw std::runtime_error("Projectile flight values must be numeric.");
            ProjectileAssetDocument d;
            d.projectId=j.at("project_id").get<std::string>();
            d.assetId=j.at("asset_id").get<std::string>();
            d.name=j.at("name").get<std::string>();
            d.speedMetresPerSecond=j.at("speed_metres_per_second").get<float>();
            d.gravityScale=j.at("gravity_scale").get<float>();
            d.lifetimeSeconds=j.at("lifetime_seconds").get<float>();
            d.damage=j.at("damage").get<float>();
            if (j.at("schema_version") >= 2) {
                if (!j.at("visual_scale").is_number() ||
                    !j.at("visual_rotation_degrees").is_array() ||
                    j.at("visual_rotation_degrees").size() != 3)
                    throw std::runtime_error("Invalid projectile appearance values.");
                for (const auto& v : j.at("visual_rotation_degrees"))
                    if (!v.is_number()) throw std::runtime_error("Rotation must be numeric.");
                d.meshAssetId = j.at("mesh_asset_id").get<std::string>();
                d.visualScale = j.at("visual_scale").get<float>();
                d.visualRotationDegrees = j.at("visual_rotation_degrees").get<std::array<float,3>>();
            }
            if(j.at("schema_version")==3) {
                const auto& effects=j.at("flight_effects");
                if(!effects.is_array()||effects.size()>2)throw std::runtime_error("Invalid flight effect list.");
                for(const auto& e:effects) {
                    if(!e.is_object()||e.size()!=5||!e.at("kind").is_number_unsigned()||
                       !e.at("offset").is_array()||e.at("offset").size()!=3 ||
                       !e.at("size_metres").is_number()||!e.at("rate").is_number()||!e.at("life_seconds").is_number())
                        throw std::runtime_error("Invalid projectile effect layer.");
                    ProjectileEffectLayer layer;
                    layer.kind=static_cast<ProjectileEffectKind>(e.at("kind").get<unsigned>());
                    layer.offset=e.at("offset").get<std::array<float,3>>();
                    layer.sizeMetres=e.at("size_metres").get<float>();layer.particlesPerSecond=e.at("rate").get<float>();
                    layer.particleLifeSeconds=e.at("life_seconds").get<float>();d.flightEffects.push_back(layer);
                }
                d.impactEffect=static_cast<ProjectileEffectKind>(j.at("impact_effect").get<unsigned>());
                d.stickOnImpact=j.at("stick_on_impact").get<bool>();
                d.stuckLifetimeSeconds=j.at("stuck_lifetime_seconds").get<float>();
                d.embedDepthMetres=j.at("embed_depth_metres").get<float>();
            }
            if (!ValidateProjectileAsset(d, error)) return false;
            out=std::move(d); error.clear(); return true;
        }
        catch (const std::exception& e) { error=e.what(); return false; }
    }

    bool ReadProjectileAssetFile(const std::string& path, ProjectileAssetDocument& d, std::string& error)
    {
        std::ifstream in(fs::u8path(path), std::ios::binary);
        if (!in) { error="Cannot read projectile asset."; return false; }
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        if (in.bad()) { error="Cannot read complete projectile asset."; return false; }
        return DeserializeProjectileAsset(text,d,error);
    }

    bool LoadProjectileAsset(const std::string& root, const StableId& project, const StableId& id,
                             ProjectileAssetDocument& out, std::string& error)
    {
        AssetRegistry registry;
        if (!ReadAssetRegistry(root,project,registry,error)) return false;
        const auto* record=Find(registry,id);
        if (!record || !record->sourceAvailable || record->dependencyClass!=DependencyClass::Data ||
            (record->provider!="renegade.projectile" && record->provider!="lp07.rasset") ||
            fs::u8path(record->projectRelativePath).extension()!=ProjectileAssetExtension)
        { error="Projectile is missing or invalid in the project registry."; return false; }
        const auto path=ResolveDependencyPath(root,record->projectRelativePath);
        ProjectileAssetDocument d;
        if (!path.accepted || !path.exists)
        { error="Projectile file is missing or outside the project."; return false; }
        if (!ReadProjectileAssetFile(path.absolutePath,d,error)) return false;
        if (d.projectId!=project || d.assetId!=id)
        { error="Projectile identity differs from its project registry."; return false; }
        if (record->dependencyAssetIds != VisualDependencies(d) ||
            !ValidateMesh(root, project, registry, d, error)) {
            if (error.empty()) error = "Projectile mesh dependencies differ from its registry.";
            return false;
        }
        out=std::move(d); error.clear(); return true;
    }

    ProjectileAssetSaveResult SaveProjectileAsset(const std::string& root, const StableId& project,
        ProjectileAssetDocument d, ProjectDocumentTransactionHook hook)
    {
        ProjectileAssetSaveResult r;
        AssetRegistry registry;
        if (!ReadAssetRegistry(root,project,registry,r.error)) return r;
        d.projectId=project; d.assetId=GenerateStableId(); r.document=std::move(d);
        std::string text;
        if (!SerializeProjectileAsset(r.document,text,r.error) ||
            !ValidateMesh(root, project, registry, r.document, r.error)) return r;
        std::error_code ec;
        const auto canonical=fs::weakly_canonical(fs::u8path(root),ec);
        if (ec || !fs::is_directory(canonical))
        { r.error="Projectile project root is unavailable."; return r; }
        r.projectRelativePath="Content/Projectiles/"+r.document.assetId+ProjectileAssetExtension;
        const auto admitted=ResolveDependencyPath(canonical.generic_u8string(),r.projectRelativePath);
        if (!admitted.accepted) { r.error=admitted.error; return r; }
        const auto destination=canonical/fs::u8path(r.projectRelativePath);
        fs::create_directories(destination.parent_path(),ec);
        if (ec) { r.error="Cannot create projectile directory."; return r; }
        AssetRecord record;
        record.assetId=r.document.assetId;
        record.dependencyNodeId="projectile:"+record.assetId;
        record.projectRelativePath=r.projectRelativePath;
        record.provider="renegade.projectile"; record.contentHash=Hash(text);
        record.dependencyAssetIds=VisualDependencies(r.document);
        registry.records.push_back(record);
        std::string registryText,registryPath;
        if (!SerializeAssetRegistry(registry,registryText,r.error) ||
            !ResolveAssetRegistryDocumentPath(canonical.generic_u8string(),registryPath,r.error)) return r;
        ProjectDocumentWrite asset{destination.generic_u8string(),{text.begin(),text.end()},
            [text](const std::string& path,std::string& error) {
                ProjectileAssetDocument d; std::string canonicalText;
                return ReadProjectileAssetFile(path,d,error) &&
                    SerializeProjectileAsset(d,canonicalText,error) && canonicalText==text;
            }};
        ProjectDocumentWrite index{registryPath,{registryText.begin(),registryText.end()},
            [project,id=record.assetId](const std::string& path,std::string& error) {
                std::ifstream in(fs::u8path(path),std::ios::binary);
                const std::string text((std::istreambuf_iterator<char>(in)),{});
                AssetRegistry registry;
                return DeserializeAssetRegistry(text,registry,error) &&
                    registry.projectId==project && Find(registry,id);
            }};
        ProjectDocumentTransactionOptions options;
        options.allowedRoot=canonical.generic_u8string();
        options.journalDirectory=(canonical/"Intermediate/Transactions").generic_u8string();
        options.operationHook=std::move(hook);
        r.transaction=ProjectDocumentTransaction().Execute({std::move(asset),std::move(index)},options);
        r.succeeded=r.transaction.success;
        if (!r.succeeded) r.error=r.transaction.message;
        return r;
    }

    std::vector<ProjectileAssetDocument> ListProjectileAssets(
        const std::string& root,const StableId& project,std::string& error)
    {
        AssetRegistry registry; std::vector<ProjectileAssetDocument> result;
        if (!ReadAssetRegistry(root,project,registry,error)) return result;
        for (const auto& record:registry.records)
        {
            if ((record.provider!="renegade.projectile" && record.provider!="lp07.rasset") ||
                fs::u8path(record.projectRelativePath).extension()!=ProjectileAssetExtension) continue;
            ProjectileAssetDocument d;
            if (!LoadProjectileAsset(root,project,record.assetId,d,error)) return {};
            result.push_back(std::move(d));
        }
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
            return a.name==b.name ? a.assetId<b.assetId : a.name<b.name;
        });
        error.clear(); return result;
    }

    ProjectileDefinition ProjectileSimulationDefinition(const ProjectileAssetDocument& d) noexcept
    {
        ProjectileDefinition result;
        result.acceleration={0,-9.81f*d.gravityScale,0};
        result.lifetimeSeconds=d.lifetimeSeconds;
        result.damage=d.damage;
        return result;
    }
}
