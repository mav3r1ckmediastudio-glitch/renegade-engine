#include "renegade/bridge/PlayerPrefabService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iterator>

namespace renegade::bridge
{
    namespace
    {
        using json = nlohmann::json;
        namespace fs = std::filesystem;
        struct Field { const char* name; float PlayerControllerSettings::*member; };
        constexpr Field Fields[] = {
            {"walk_speed",&PlayerControllerSettings::walkSpeed},
            {"sprint_speed",&PlayerControllerSettings::sprintSpeed},
            {"jump_speed",&PlayerControllerSettings::jumpSpeed},
            {"look_sensitivity",&PlayerControllerSettings::lookSensitivity},
            {"minimum_pitch",&PlayerControllerSettings::minimumPitch},
            {"maximum_pitch",&PlayerControllerSettings::maximumPitch},
            {"capsule_radius",&PlayerControllerSettings::capsuleRadius},
            {"capsule_half_height",&PlayerControllerSettings::capsuleHeight},
            {"eye_height",&PlayerControllerSettings::eyeHeight},
            {"maximum_slope",&PlayerControllerSettings::maximumSlopeDegrees},
            {"gravity_factor",&PlayerControllerSettings::gravityFactor},
        };
        const AssetRecord* Find(const AssetRegistry& registry, const StableId& id)
        {
            auto it=std::find_if(registry.records.begin(),registry.records.end(),
                [&](const AssetRecord& record){return record.assetId==id;});
            return it==registry.records.end()?nullptr:&*it;
        }
        bool Validate(const PlayerPrefabDocument& d, std::string& error)
        {
            if(!IsValidStableId(d.projectId)||!IsValidStableId(d.assetId)||
                d.name.empty()||d.name.size()>96||
                !PlayerSettingsEqual(d.settings,SanitizePlayerControllerSettings(d.settings)))
            { error="Player prefab contains invalid identity, name or controller settings."; return false; }
            error.clear(); return true;
        }
        std::string Hash(const std::string& text)
        {
            uint64_t hash=1469598103934665603ull;
            for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}
            std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;
            return out.str();
        }
        bool ArmsAvailable(const std::string& root, const AssetRegistry& registry, const StableId& id, std::string& error)
        {
            if(id.empty())return true;
            const auto* record=Find(registry,id);
            if(!record||!record->sourceAvailable||
                record->dependencyClass!=DependencyClass::ImportedContent||
                fs::u8path(record->projectRelativePath).extension()!=ReusableAssetExtension)
            {error="Player prefab arms must resolve to an available registered assembly asset.";return false;}
            const auto path=ResolveDependencyPath(root,record->projectRelativePath);
            if(!path.accepted||!path.exists)
            {error="Player prefab arms file is missing or outside the project.";return false;}
            return true;
        }
        void SetString(wi::scene::MetadataComponent& m,const char* key,const std::string& value)
        {if(value.empty())m.string_values.erase(key);else m.string_values.set(key,value);}
    }
    bool PlayerSettingsEqual(const PlayerControllerSettings& a,const PlayerControllerSettings& b)
    {
        for(const auto& f:Fields)
            if(!std::isfinite(a.*f.member)||!std::isfinite(b.*f.member)||
                std::abs(a.*f.member-b.*f.member)>0.00001f)return false;
        return a.firstPersonArmsAssetId==b.firstPersonArmsAssetId;
    }
    bool SerializePlayerPrefab(const PlayerPrefabDocument& d,std::string& text,std::string& error)
    {
        if(!Validate(d,error))return false;
        json values=json::object();
        for(const auto& f:Fields)values[f.name]=d.settings.*f.member;
        values["first_person_arms_asset_id"]=d.settings.firstPersonArmsAssetId;
        text=json{{"format","renegade-player-prefab"},{"schema_version",1},
            {"project_id",d.projectId},{"asset_id",d.assetId},{"name",d.name},{"settings",values}}.dump(2);
        return true;
    }
    bool DeserializePlayerPrefab(const std::string& text,PlayerPrefabDocument& out,std::string& error)
    {
        try
        {
            auto j=json::parse(text);
            if(!j.is_object()||j.size()!=6||j.at("format")!="renegade-player-prefab"||
                !j.at("schema_version").is_number_integer()||j.at("schema_version")!=1||
                !j.at("project_id").is_string()||!j.at("asset_id").is_string()||
                !j.at("name").is_string())throw std::runtime_error("Unsupported player prefab schema.");
            PlayerPrefabDocument d;
            d.projectId=j.at("project_id").get<std::string>();
            d.assetId=j.at("asset_id").get<std::string>();
            d.name=j.at("name").get<std::string>();
            auto& values=j.at("settings");
            if(!values.is_object()||values.size()!=12)throw std::runtime_error("Incomplete player settings.");
            for(const auto& f:Fields)
            {
                if(!values.at(f.name).is_number())throw std::runtime_error("Player setting must be numeric.");
                d.settings.*f.member=values.at(f.name).get<float>();
            }
            if(!values.at("first_person_arms_asset_id").is_string())throw std::runtime_error("Malformed arms identity.");
            d.settings.firstPersonArmsAssetId=values.at("first_person_arms_asset_id").get<std::string>();
            if(!Validate(d,error))return false;
            out=std::move(d);error.clear();return true;
        }
        catch(const std::exception& e){error=e.what();return false;}
    }
    bool ReadPlayerPrefabFile(const std::string& path,PlayerPrefabDocument& d,std::string& error)
    {
        std::ifstream in(fs::u8path(path),std::ios::binary);
        if(!in){error="Cannot read player prefab.";return false;}
        std::string text((std::istreambuf_iterator<char>(in)),{});
        if(!in.eof()&&in.fail()){error="Cannot read complete player prefab.";return false;}
        return DeserializePlayerPrefab(text,d,error);
    }
    bool LoadPlayerPrefab(const std::string& root,const StableId& project,const StableId& id,
        PlayerPrefabDocument& d,std::string& error)
    {
        AssetRegistry registry;
        if(!ReadAssetRegistry(root,project,registry,error))return false;
        const auto* record=Find(registry,id);
        if(!record||!record->sourceAvailable||record->dependencyClass!=DependencyClass::Data||
            fs::u8path(record->projectRelativePath).extension()!=PlayerPrefabExtension)
        {error="Player prefab is missing from the project registry.";return false;}
        const auto path=ResolveDependencyPath(root,record->projectRelativePath);
        if(!path.accepted||!path.exists){error="Player prefab file is missing or outside the project.";return false;}
        PlayerPrefabDocument loaded;
        if(!ReadPlayerPrefabFile(path.absolutePath,loaded,error))return false;
        if(loaded.projectId!=project||loaded.assetId!=id)
        {error="Player prefab identity does not match its registered project.";return false;}
        if(!ArmsAvailable(root,registry,loaded.settings.firstPersonArmsAssetId,error))return false;
        d=std::move(loaded);return true;
    }
    PlayerPrefabSaveResult SavePlayerPrefab(const std::string& projectRoot,const StableId& project,
        const std::string& name,const PlayerControllerSettings& settings,ProjectDocumentTransactionHook hook)
    {
        PlayerPrefabSaveResult r;
        AssetRegistry registry;
        if(!ReadAssetRegistry(projectRoot,project,registry,r.error)||
            !ArmsAvailable(projectRoot,registry,settings.firstPersonArmsAssetId,r.error))return r;
        r.document={project,GenerateStableId(),name,settings};
        std::string text;
        if(!SerializePlayerPrefab(r.document,text,r.error))return r;
        std::error_code ec;
        auto root=fs::weakly_canonical(fs::u8path(projectRoot),ec);
        if(ec||!fs::is_directory(root)){r.error="Player prefab project root is unavailable.";return r;}
        // ID-based filenames avoid title collisions and path injection.
        r.projectRelativePath="Content/Players/"+r.document.assetId+PlayerPrefabExtension;
        const auto destination=root/fs::u8path(r.projectRelativePath);
        fs::create_directories(destination.parent_path(),ec);
        if(ec){r.error="Cannot create player prefab directory.";return r;}
        AssetRecord record;
        record.assetId=r.document.assetId;record.dependencyNodeId="player-prefab:"+record.assetId;
        record.projectRelativePath=r.projectRelativePath;record.provider="renegade.player_prefab";
        record.contentHash=Hash(text);
        if(!settings.firstPersonArmsAssetId.empty())record.dependencyAssetIds={settings.firstPersonArmsAssetId};
        registry.records.push_back(record);
        std::string registryText,registryPath;
        if(!SerializeAssetRegistry(registry,registryText,r.error)||
            !ResolveAssetRegistryDocumentPath(root.generic_u8string(),registryPath,r.error))return r;
        ProjectDocumentWrite prefab{destination.generic_u8string(),{text.begin(),text.end()},
            [expected=r.document](const std::string& path,std::string& error)
            {
                PlayerPrefabDocument d;
                return ReadPlayerPrefabFile(path,d,error)&&d.projectId==expected.projectId&&
                    d.assetId==expected.assetId&&d.name==expected.name&&
                    PlayerSettingsEqual(d.settings,expected.settings);
            }};
        ProjectDocumentWrite index{registryPath,{registryText.begin(),registryText.end()},
            [project,id=record.assetId](const std::string& path,std::string& error)
            {
                std::ifstream in(fs::u8path(path),std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(in)),{});
                AssetRegistry parsed;
                return DeserializeAssetRegistry(text,parsed,error)&&parsed.projectId==project&&Find(parsed,id);
            }};
        ProjectDocumentTransactionOptions options;
        options.allowedRoot=root.generic_u8string();
        options.journalDirectory=(root/"Intermediate/Transactions").generic_u8string();
        options.operationHook=std::move(hook);
        r.transaction=ProjectDocumentTransaction().Execute({std::move(prefab),std::move(index)},options);
        r.succeeded=r.transaction.success;
        if(!r.succeeded)r.error=r.transaction.message;
        return r;
    }
    std::vector<PlayerPrefabDocument> ListPlayerPrefabs(const std::string& root,
        const StableId& project,std::string& error)
    {
        std::vector<PlayerPrefabDocument> result;AssetRegistry registry;
        if(!ReadAssetRegistry(root,project,registry,error))return result;
        for(const auto& record:registry.records)
        {
            if(fs::u8path(record.projectRelativePath).extension()!=PlayerPrefabExtension)continue;
            PlayerPrefabDocument d;
            if(!LoadPlayerPrefab(root,project,record.assetId,d,error))return {};
            result.push_back(std::move(d));
        }
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.name<b.name;});
        error.clear();return result;
    }
    StableId CapturePlayerPrefabOrigin(const wi::scene::Scene& scene,wi::ecs::Entity entity)
    {
        const auto* m=scene.metadatas.GetComponent(entity);
        return m&&m->string_values.has(PlayerPrefabOriginKey)?m->string_values.get(PlayerPrefabOriginKey):"";
    }
    bool CapturePlayerPrefabBaseline(const wi::scene::Scene& scene,wi::ecs::Entity entity,
        PlayerPrefabDocument& d,std::string& error)
    {
        const auto* m=scene.metadatas.GetComponent(entity);
        if(!IsPlayerStart(scene,entity)||!m||!m->string_values.has(PlayerPrefabBaselineKey))
        {error="Player Start has no prefab baseline.";return false;}
        if(!DeserializePlayerPrefab(m->string_values.get(PlayerPrefabBaselineKey),d,error))return false;
        if(d.assetId!=CapturePlayerPrefabOrigin(scene,entity))
        {error="Player prefab baseline identity mismatch.";return false;}
        return true;
    }
    ApplyPlayerPrefabCommand::ApplyPlayerPrefabCommand(wi::scene::Scene& scene,
        wi::ecs::Entity entity,PlayerPrefabDocument document)
        :scene_(&scene),entity_(entity),document_(std::move(document)){}
    bool ApplyPlayerPrefabCommand::Execute()
    {
        std::string error;
        if(!scene_||!IsPlayerStart(*scene_,entity_)||
            !SerializePlayerPrefab(document_,afterBaseline_,error))return false;
        auto* m=scene_->metadatas.GetComponent(entity_);
        if(!captured_)
        {
            beforeOrigin_=CapturePlayerPrefabOrigin(*scene_,entity_);
            beforeBaseline_=m->string_values.has(PlayerPrefabBaselineKey)?
                m->string_values.get(PlayerPrefabBaselineKey):"";
            if(beforeOrigin_==document_.assetId&&beforeBaseline_==afterBaseline_&&
                PlayerSettingsEqual(CapturePlayerControllerSettings(*scene_,entity_),document_.settings))return false;
            settings_=std::make_unique<SetPlayerControllerSettingsCommand>(*scene_,entity_,document_.settings);
            captured_=true;
        }
        // A settings no-op is valid when only the prefab identity is changing.
        (void)settings_->Execute();
        SetString(*m,PlayerPrefabOriginKey,document_.assetId);
        SetString(*m,PlayerPrefabBaselineKey,afterBaseline_);
        return true;
    }
    void ApplyPlayerPrefabCommand::Undo()
    {
        if(!captured_||!IsPlayerStart(*scene_,entity_))return;
        settings_->Undo();
        auto* m=scene_->metadatas.GetComponent(entity_);
        SetString(*m,PlayerPrefabOriginKey,beforeOrigin_);
        SetString(*m,PlayerPrefabBaselineKey,beforeBaseline_);
    }
}
