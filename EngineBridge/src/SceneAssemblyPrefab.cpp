#include "renegade/bridge/SceneAssemblyService.h"
#include "renegade/bridge/SceneAssemblyScriptService.h"
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
namespace renegade::bridge {
namespace {
namespace fs=std::filesystem;
using wi::ecs::Entity;
std::vector<std::uint8_t> Bytes(const std::string& s){return {s.begin(),s.end()};}
bool Read(const fs::path& p,std::vector<std::uint8_t>& b){
    std::ifstream f(p,std::ios::binary);if(!f)return false;
    b.assign(std::istreambuf_iterator<char>(f),{});return !f.bad();
}
std::string Hash(const std::vector<std::uint8_t>& bytes){
    std::uint64_t h=1469598103934665603ull;for(auto v:bytes){h^=v;h*=1099511628211ull;}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return out.str();
}
ProjectDocumentWrite Write(const fs::path& path,const std::vector<std::uint8_t>& bytes){
    ProjectDocumentWrite w;w.destinationPath=path.generic_u8string();w.content=bytes;
    w.validator=[bytes](const std::string& path,std::string& error){
        std::vector<std::uint8_t> actual;if(!Read(fs::u8path(path),actual)||actual!=bytes){error="Prefab staged bytes changed.";return false;}return true;
    };return w;
}
bool PrepareAssembly(wi::scene::Scene& source,const std::vector<Entity>& selected,
    wi::scene::Scene& result,std::string& error,const std::string& projectRoot,const StableId& project,const ScriptDocument* scripts) {
    const auto roots=SelectionRoots(source,selected);
    if(roots.empty()){error="Select pieces or a group first.";return false;}
    std::set<Entity> members;
    for(auto root:roots) {
        members.insert(root);wi::vector<Entity> children;source.GatherChildren(root,children);
        members.insert(children.begin(),children.end());
    }
    // One shared native serializer preserves all intra-assembly references.
    wi::ecs::EntitySerializer reader;
    for(auto root:roots)if(auto* h=source.hierarchy.GetComponent(root))
        if(!members.count(h->parentID))reader.remap[h->parentID]=wi::ecs::INVALID_ENTITY;
    std::set<Entity> copied;
    auto copy=[&](Entity e) {
        wi::Archive archive;wi::ecs::EntitySerializer writer;
        source.Entity_Serialize(archive,writer,e,wi::scene::Scene::EntitySerializeFlags::NONE);
        wi::jobsystem::Wait(writer.ctx);archive.SetReadModeAndResetPos(true);
        auto clone=result.Entity_Serialize(archive,reader,wi::ecs::INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::NONE);
        wi::jobsystem::Wait(reader.ctx);copied.insert(e);
        if(auto* material=source.materials.GetComponent(e))
            if(auto* target=result.materials.GetComponent(clone))
                for(size_t slot=0;slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT;++slot)
                    target->textures[slot]=material->textures[slot];
    };
    for(auto e:members)copy(e);
    auto copyResource=[&](Entity e) {
        const auto clone=reader.remap.at(e);
        auto component=[&](const char* name,auto& from,auto& to) {
            if(!from.Contains(e))return;
            wi::Archive archive;wi::ecs::EntitySerializer writer;
            writer.componentlibrary=&source.componentLibrary;
            for(const auto& entry:source.componentLibrary.entries)writer.library_versions[entry.first]=entry.second.version;
            writer.version=source.componentLibrary.GetVersion(name);
            from.Component_Serialize(e,archive,writer);wi::jobsystem::Wait(writer.ctx);
            archive.SetReadModeAndResetPos(true);
            reader.componentlibrary=&result.componentLibrary;reader.version=writer.version;
            reader.library_versions=writer.library_versions;
            to.Component_Serialize(clone,archive,reader);wi::jobsystem::Wait(reader.ctx);
        };
        component("wi::scene::Scene::meshes",source.meshes,result.meshes);
        component("wi::scene::Scene::materials",source.materials,result.materials);
        component("wi::scene::Scene::animation_datas",source.animation_datas,result.animation_datas);
        if(auto* material=source.materials.GetComponent(e)) {
            auto* target=result.materials.GetComponent(clone);
            for(size_t slot=0;slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT;++slot)target->textures[slot]=material->textures[slot];
            if(auto* metadata=source.metadatas.GetComponent(e)) {
                // Preserve typed material dependencies, not unrelated donor-object metadata.
                for(auto slot:{MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,
                    MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion}) {
                    const auto* key=MaterialTextureSlotMetadataKey(slot);if(!metadata->string_values.has(key))continue;
                    auto* targetMetadata=result.metadatas.GetComponent(clone);
                    if(!targetMetadata)targetMetadata=&result.metadatas.Create(clone);
                    targetMetadata->string_values.set(key,metadata->string_values.get(key));
                    targetMetadata->int_values.set(MaterialTextureAssetBindingVersionMetadataKey,MaterialTextureAssetBindingVersion);
                }
            }
        }
        copied.insert(e);
    };
    // The read-side native remap discovers referenced mesh/material/animation data.
    // Follow it to closure; external authored objects are rejected, never copied silently.
    for(;;) {
        std::vector<Entity> pending;
        for(const auto& mapping:reader.remap)
            if(mapping.second!=wi::ecs::INVALID_ENTITY && !copied.count(mapping.first))pending.push_back(mapping.first);
        if(pending.empty())break;
        for(auto e:pending) {
            if(!source.meshes.Contains(e)&&!source.materials.Contains(e)&&!source.animation_datas.Contains(e)) {
                error="A selected component references an object outside this assembly. Include that object before saving.";return false;
            }copyResource(e);
        }
    }
    std::vector<Entity> clonedRoots;
    for(auto root:roots) {
        const auto clone=reader.remap.at(root);clonedRoots.push_back(clone);
        result.hierarchy.Remove(clone);
        auto* t=result.transforms.GetComponent(clone);t->ClearTransform();
        t->MatrixTransform(AssemblyWorldMatrix(source,root));t->UpdateTransform();
    }
    Entity root;
    if(clonedRoots.size()==1 && IsSceneAssembly(result,clonedRoots.front()))root=clonedRoots.front();
    else {
        root=result.Entity_CreateTransform("Prefab");
        XMVECTOR center=XMVectorZero();for(auto e:clonedRoots)center+=result.transforms.GetComponent(e)->GetPositionV();
        center/=float(clonedRoots.size());
        auto* t=result.transforms.GetComponent(root);XMStoreFloat3(&t->translation_local,center);t->SetDirty();t->UpdateTransform();
        for(auto e:clonedRoots)result.Component_Attach(e,root);
    }
    SceneEntityPairs pairs;
    for(auto e:members)pairs.emplace_back(e,reader.remap.at(e));
    if(!CaptureSceneAssemblyScripts(projectRoot,project,source,scripts,pairs,result,root,error))return false;
    // Retain a useful authored name if this root carries behaviour when unpacked.
    auto* authoredMetadata=result.metadatas.GetComponent(root);
    if(!authoredMetadata)authoredMetadata=&result.metadatas.Create(root);
    authoredMetadata->string_values.set("renegade.scene_assembly.authored_name",result.names.GetComponent(root)->name);
    // Creator-authored root is recognized by the existing placement pipeline.
    result.names.GetComponent(root)->name="__renegade_creator_authored_transform";
    auto* metadata=result.metadatas.GetComponent(root);if(!metadata)metadata=&result.metadatas.Create(root);
    metadata->string_values.set(SceneAssemblyKey,"1");
    // Place the outer authoring root before nested imported model roots.
    result.names.MoveItem(result.names.GetIndex(root),0);
    result.transforms.MoveItem(result.transforms.GetIndex(root),0);
    auto* t=result.transforms.GetComponent(root);t->translation_local={};t->SetDirty();t->UpdateTransform();
    error.clear();return true;
}
}
bool SaveSceneAssemblyPrefab(const std::string& projectRoot,const StableId& project,
    wi::scene::Scene& source,const std::vector<Entity>& selected,const std::string& name,StableId& asset,std::string& error,const ScriptDocument* scripts) {
    asset.clear();
    if(name.empty()||name.size()>80||name=="."||name==".."||name.back()=='.'||name.back()==' '||
        name.find_first_of("<>:\"/\\|?*")!=std::string::npos||
        std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;})) {
        error="Choose a valid new prefab name.";return false;
    }
    std::error_code ec;const auto root=fs::weakly_canonical(fs::u8path(projectRoot),ec);
    if(ec||!fs::is_directory(root)||!IsValidStableId(project)){error="Open a project before saving a prefab.";return false;}
    wi::scene::Scene scene;if(!PrepareAssembly(source,selected,scene,error,projectRoot,project,scripts))return false;
    AssetRegistry registry;if(!ReadAssetRegistry(projectRoot,project,registry,error))return false;
    const auto id=GenerateStableId(),sourceId=GenerateStableId();
    const std::string relative="Content/Prefabs/"+name+".rasset";
    const std::string sourceRelative="SourceAssets/Prefabs/"+name+".json";
    const auto path=root/fs::u8path(relative),recipePath=root/fs::u8path(sourceRelative);
    const auto projection=root/fs::u8path(ResolveReusableModelManagedProjectionPath(relative));
    for(const auto& p:{path,recipePath,projection}) {
        if(fs::exists(p)){error="A prefab with that name already exists. Choose a new name.";return false;}
        fs::create_directories(p.parent_path(),ec);if(ec){error=ec.message();return false;}
        auto canonical=fs::weakly_canonical(p.parent_path(),ec);auto rel=canonical.lexically_relative(root);
        if(ec||rel.empty()||*rel.begin()==".."){error="Prefab destination escapes the project.";return false;}
    }
    bool scripted=false;
    for(size_t i=0;i<scene.metadatas.GetCount();++i)
        scripted|=scene.metadatas[i].string_values.has(SceneAssemblyScriptTemplateKey);
    // Older geometry-only readers reject version 2 instead of silently dropping behaviour.
    const std::string recipe=std::string("{\"options\":{\"schema_version\":")+
        (scripted?"2":"1")+"},\"source_format\":\"scene-assembly\"}";
    ReusableModelAssetDocument document;auto& manifest=document.manifest;
    manifest.projectId=project;manifest.assetId=id;manifest.sourceAssetId=sourceId;
    manifest.sourceFormat="scene-assembly";manifest.importer="renegade.scene.assembly";manifest.settingsJson=recipe;
    struct RestoreMode{wi::resourcemanager::Mode before=wi::resourcemanager::GetMode();~RestoreMode(){wi::resourcemanager::SetMode(before);}} restore;
    wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
    auto staged=root/"Intermediate/Imports"/fs::u8path(".prefab-"+id+".wiscene");fs::create_directories(staged.parent_path(),ec);
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove(path,ec);}} cleanup{staged};
    wi::Archive archive(staged.generic_u8string(),false,false);if(!archive.IsOpen()){error="Could not stage prefab.";return false;}
    archive.SetCompressionEnabled(true);scene.Serialize(archive);
    const bool saved=archive.SaveFile(staged.generic_u8string());archive=wi::Archive();
    if(!saved||!Read(staged,document.payload)){error="Could not retain prefab payload.";return false;}
    manifest.payloadHash=Hash(document.payload);
    std::vector<std::uint8_t> product;if(!SerializeReusableModelAssetDocument(document,product,error))return false;
    ReusableModelManagedProjection p;p.projectId=project;p.assetId=id;p.sourceAssetId=sourceId;
    p.sourceProjectRelativePath=sourceRelative;p.assetProjectRelativePath=relative;p.sourceFormat=manifest.sourceFormat;
    p.importer=manifest.importer;p.settingsJson=recipe;p.payloadHash=manifest.payloadHash;
    p.modelMetadata=FirstPersonAssemblyService().Describe(scene);
    std::string projectionJson;if(!SerializeReusableModelManagedProjection(p,projectionJson,error))return false;
    auto add=[&](const StableId& id,const std::string& path,const std::string& hash,bool source){
        AssetRecord record;record.assetId=id;record.dependencyNodeId=std::string(source?"lp07.source:":"lp07.rasset:")+id;
        record.projectRelativePath=path;record.dependencyClass=DependencyClass::ImportedContent;
        record.requirement=source?DependencyRequirement::EditorOnly:DependencyRequirement::Required;
        record.provider=source?"lp07.source_asset":"lp07.rasset";record.contentHash=hash;registry.records.push_back(record);
    };
    add(sourceId,sourceRelative,Hash(Bytes(recipe)),true);add(id,relative,Hash(product),false);
    ImportedProductRecord provenance;provenance.sourceAssetId=sourceId;provenance.productAssetId=id;
    provenance.importer=manifest.importer;provenance.settingsSchema=manifest.settingsSchema;provenance.settingsJson=recipe;
    provenance.sourceContentHashAtImport=Hash(Bytes(recipe));provenance.productContentHashAtImport=Hash(product);
    registry.importedProducts.push_back(provenance);
    if(!ValidateAssetRegistry(registry,error))return false;
    AssetCatalogueMetadataDocument catalogue;
    if(!ReadAssetCatalogueMetadata(projectRoot,project,catalogue,error)||!SetAssetModelDerivedMetadata(catalogue,id,p.modelMetadata,error))return false;
    std::string registryJson,catalogueJson;
    if(!SerializeAssetRegistry(registry,registryJson,error)||!SerializeAssetCatalogueMetadata(catalogue,catalogueJson,error))return false;
    ProjectDocumentTransactionOptions options;options.allowedRoot=root.generic_u8string();
    options.journalDirectory=(root/"Intermediate/Transactions").generic_u8string();
    auto transaction=ProjectDocumentTransaction().Execute({
        Write(recipePath,Bytes(recipe)),Write(path,product),Write(projection,Bytes(projectionJson)),
        Write(root/AssetRegistryDocumentName,Bytes(registryJson)),Write(root/AssetCatalogueMetadataDocumentName,Bytes(catalogueJson))},options);
    if(!transaction.success){error=transaction.message;return false;}
    asset=id;error.clear();return true;
}
}
