#include "renegade/bridge/ImpactDefaultsService.h"
#include "renegade/bridge/ImpactAudioService.h"
#include <filesystem>
#include <fstream>
#include <map>

namespace renegade::bridge
{
    namespace fs=std::filesystem;
    bool IsImpactDefaultsMaterialKey(const std::string& key)
    {
        return key=="renegade.blood_sheet" || key=="renegade.impact.glass_shards" ||
            key=="renegade.impact.wood_shards" || key=="renegade.impact.rock_shards" ||
            key.rfind("renegade.impact.mark.",0)==0;
    }
    namespace
    {
        struct Donor
        {
            wi::ecs::Entity entity=wi::ecs::CreateEntity();
            std::string key,name;
            wi::scene::MaterialComponent material;
            wi::scene::MetadataComponent metadata;
        };
        bool HasKey(const wi::scene::Scene& scene,const std::string& key)
        {
            for(size_t i=0;i<scene.metadatas.GetCount();++i)
                if(scene.metadatas[i].bool_values.has(key) && scene.metadatas[i].bool_values.get(key))return true;
            return false;
        }
        class Adopt final:public ICommand
        {
            wi::scene::Scene* scene_;
            std::vector<Donor> donors_;
            std::vector<wi::ecs::Entity> created_;
        public:
            Adopt(wi::scene::Scene& scene,std::vector<Donor> donors):scene_(&scene),donors_(std::move(donors)){}
            bool Execute() override
            {
                created_.clear();
                for(const auto& donor:donors_)if(!HasKey(*scene_,donor.key)) {
                    scene_->names.Create(donor.entity).name=donor.name;
                    scene_->transforms.Create(donor.entity);
                    scene_->materials.Create(donor.entity)=donor.material;
                    scene_->metadatas.Create(donor.entity)=donor.metadata;
                    created_.push_back(donor.entity);
                }
                return !created_.empty();
            }
            void Undo() override
            {
                for(const auto entity:created_)scene_->Entity_Remove(entity);
                created_.clear();
            }
        };
        bool Import(const ProjectMetadata& target,const StableId& sourceId,
            ResourceSourceFormat format,const std::vector<uint8_t>& payload,
            StableId& id,std::string& error)
        {
            std::string extension;
            for(const auto& capability:GetSupportedResourceFormats())if(capability.format==format)extension=capability.extension;
            if(extension.empty()){error="Unsupported impact library resource.";return false;}
            if(extension.front()!='.')extension="."+extension;
            const std::string stem=std::string(format==ResourceSourceFormat::Wav?"Audio":"Textures")+"/Impacts/Defaults/"+sourceId;
            ResourceAssetImportRequest request;request.projectRoot=target.rootPath;request.projectId=target.projectId;
            request.sourceProjectRelativePath="SourceAssets/"+stem+extension;
            request.assetProjectRelativePath="Content/"+stem+".rasset";request.expectedFormat=format;
            const auto product=fs::u8path(target.rootPath)/request.assetProjectRelativePath;
            if(fs::exists(product)) {
                ResourceAssetDocument existing;
                if(!ReadResourceAssetDocument(product.generic_u8string(),existing,error))return false;
                if(existing.manifest.projectId!=target.projectId || existing.payload!=payload) {
                    error="Existing impact default resource differs; it was preserved.";return false;
                }
                id=existing.manifest.assetId;return true;
            }
            const auto source=fs::u8path(target.rootPath)/request.sourceProjectRelativePath;
            fs::create_directories(source.parent_path());fs::create_directories(product.parent_path());
            if(fs::exists(source)) {
                std::ifstream in(source,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
                if(bytes!=payload){error="Existing impact source differs; it was preserved.";return false;}
            } else {
                std::ofstream out(source,std::ios::binary);out.write(reinterpret_cast<const char*>(payload.data()),payload.size());
                if(!out){error="Could not write impact resource.";return false;}
            }
            const auto imported=ResourceAssetService().ImportResourceAsset(request);
            if(!imported.succeeded){error=imported.error;return false;}id=imported.assetId;return true;
        }
    }
    std::unique_ptr<ICommand> PrepareImpactDefaults(wi::scene::Scene& scene,const ProjectMetadata& target,
        const std::string& descriptor,std::string& error)
    {
        error.clear();
        try {
            ProjectMetadata source;ProjectService projects;
            if(!projects.InspectProject(descriptor,source,error))return {};
            wi::Archive archive((fs::u8path(source.rootPath)/source.startupScene).generic_u8string(),true,false);
            if(!archive.IsOpen()){error="Impact library scene is unavailable.";return {};}
            auto library=std::make_unique<wi::scene::Scene>();library->Serialize(archive);
            std::vector<Donor> donors;std::map<StableId,StableId> remap;
            for(size_t i=0;i<library->materials.GetCount();++i) {
                const auto entity=library->materials.GetEntity(i);
                const auto* metadata=library->metadatas.GetComponent(entity);if(!metadata)continue;
                std::string key;
                for(const auto& candidate:metadata->bool_values.names)
                    if(IsImpactDefaultsMaterialKey(candidate) && metadata->bool_values.get(candidate)){key=candidate;break;}
                if(key.empty() || HasKey(scene,key))continue;
                Donor donor;donor.key=key;const auto* name=library->names.GetComponent(entity);
                donor.name=name?name->name:"Impact default";
                donor.material=library->materials[i];
                for(auto& texture:donor.material.textures)texture={};
                donor.metadata.bool_values.set(key,true);
                donor.metadata.bool_values.set(ImpactDefaultsDirectionalKey,true);
                wi::scene::Scene materialScene;
                materialScene.materials.Create(donor.entity)=donor.material;
                materialScene.metadatas.Create(donor.entity)=donor.metadata;
                for(const auto slot:{MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,
                    MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion}) {
                    const auto binding=MaterialTextureSlotMetadataKey(slot);
                    if(!metadata->string_values.has(binding))continue;
                    const auto originalId=metadata->string_values.get(binding);PreparedMaterialTextureAsset original;
                    if(!PrepareMaterialTextureAsset(source.rootPath,source.projectId,originalId,original,error))return {};
                    StableId targetId;
                    if(remap.count(originalId))targetId=remap.at(originalId);
                    else {
                        if(!Import(target,originalId,original.sourceFormat,original.payload,targetId,error))return {};
                        remap[originalId]=targetId;
                    }
                    PreparedMaterialTextureAsset prepared;
                    if(!PrepareMaterialTextureAsset(target.rootPath,target.projectId,targetId,prepared,error) ||
                        !ApplyPreparedMaterialTextureAsset(materialScene,donor.entity,slot,prepared,{},error))return {};
                }
                donor.material=*materialScene.materials.GetComponent(donor.entity);
                donor.metadata=*materialScene.metadatas.GetComponent(donor.entity);donors.push_back(std::move(donor));
            }
            if(!fs::exists(fs::u8path(target.rootPath)/ImpactAudioBankPath)) {
                ImpactAudioBank bank;
                if(!ReadImpactAudioBank(source.rootPath,source.projectId,bank,error))return {};
                bank.projectId=target.projectId;
                for(auto& surface:bank.surfaces)for(auto& id:surface) {
                    std::vector<uint8_t> payload;
                    if(!PrepareImpactAudioAsset(source.rootPath,"",source.projectId,id,payload,error))return {};
                    StableId imported;
                    if(!Import(target,id,ResourceSourceFormat::Wav,payload,imported,error))return {};
                    id=imported;
                }
                if(!WriteImpactAudioBank(target.rootPath,bank,error))return {};
            }
            return std::make_unique<Adopt>(scene,std::move(donors));
        } catch(const std::exception& exception) {error=exception.what();return {};}
    }
}
