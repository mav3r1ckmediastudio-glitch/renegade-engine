#include "renegade/bridge/SceneAssemblyScriptService.h"
#include "renegade/bridge/SceneAssemblyService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace renegade::bridge;
using wi::ecs::Entity;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& label) {
    if(!ok){std::cerr<<"FAIL: "<<label<<std::endl;std::exit(1);}std::cout<<"PASS: "<<label<<std::endl;
}
static Entity NamedChild(const wi::scene::Scene& scene,Entity root,const std::string& name) {
    for(size_t i=0;i<scene.names.GetCount();++i) {
        auto e=scene.names.GetEntity(i);
        if(scene.names[i].name==name&&scene.Entity_IsDescendant(e,root))return e;
    }return wi::ecs::INVALID_ENTITY;
}
static const ScriptAttachment* Owned(const ScriptDocument& doc,const StableId& id) {
    for(const auto& row:doc.attachments)if(row.ownerEntityId==id)return &row;return nullptr;
}
static StableId Reference(const ScriptAttachment& row,const char* key) {
    for(const auto& property:row.properties)if(property.name==key)return property.referenceId;return {};
}
int main() {
    const auto project=GenerateStableId();std::string error;
    auto document=CreateScriptDocument(project,GenerateStableId(),"Content/Scenes/Core.wiscene","prefab-core");
    wi::scene::Scene scene;
    const auto root=scene.Entity_CreateTransform("Brazier");
    scene.metadatas.Create(root).string_values.set(SceneAssemblyKey,"1");
    const auto lamp=scene.Entity_CreateTransform("Lamp"),effect=scene.Entity_CreateTransform("Effect"),control=scene.Entity_CreateTransform("Controller");
    scene.lights.Create(lamp).intensity=17;
    scene.metadatas.Create(effect).string_values.set("renegade.particle.effect",GenerateStableId());
    for(auto e:{lamp,effect,control})scene.Component_Attach(e,root,true);
    const auto external=scene.Entity_CreateTransform("External actor");
    for(auto e:{root,lamp,effect,control,external})Check(AssignNewPersistentEntityId(scene,e,error),"assign independent source identity");
    ScriptSourceBinding source;source.sourceId=GenerateStableId();source.sourcePath="Content/Scripts/Prefab/Core.lua";
    auto row=CreateScriptAttachment(ScriptScope::Entity,PersistentEntityId(scene,control),source);
    for(auto pair:{std::pair<const char*,Entity>{"lamp",lamp},{"effect",effect},{"external",external}}) {
        ScriptPropertyValue p;p.name=pair.first;p.type=ScriptPropertyType::EntityReference;p.referenceId=PersistentEntityId(scene,pair.second);row.properties.push_back(p);
    }
    ScriptPropertyValue strength;strength.name="strength";strength.type=ScriptPropertyType::Float;strength.numberValue=.37f;row.properties.push_back(strength);
    ScriptPropertyValue tint;tint.name="tint";tint.type=ScriptPropertyType::Colour;tint.x=1;tint.y=.45f;tint.z=.1f;tint.w=.8f;row.properties.push_back(tint);
    row.unknownFields["future_option"]="retained";
    Check(AddScriptAttachment(document,row,error),"add typed script and component references: "+error);
    auto lightRow=CreateScriptAttachment(ScriptScope::Entity,PersistentEntityId(scene,lamp),source);
    Check(AddScriptAttachment(document,lightRow,error),"add a second component behaviour");
    std::string encoded;std::vector<ScriptAttachment> decoded;
    Check(SerializeScriptAttachmentTemplates(project,document.attachments,encoded,error)&&
        DeserializeScriptAttachmentTemplates(encoded,project,decoded,error),"typed templates round-trip in memory: "+error);
    const auto decodedController=std::find_if(decoded.begin(),decoded.end(),[&](const auto& value){return value.scriptInstanceId==row.scriptInstanceId;});
    Check(decoded.size()==2&&decodedController!=decoded.end()&&decodedController->unknownFields.at("future_option")=="retained"&&
        std::abs(decodedController->properties[3].numberValue-.37f)<.00001f&&decodedController->properties[4].w==.8f,
        "unknown metadata, numeric values and colour alpha survive templates");
    Check(!DeserializeScriptAttachmentTemplates(encoded,GenerateStableId(),decoded,error),"cross-project templates are rejected");
    Check(!DeserializeScriptAttachmentTemplates("{broken",project,decoded,error),"malformed template text is rejected");
    SetDuplicateEntityCompanionFactory([&](wi::scene::Scene& target,Entity,Entity,DuplicateEntityCompanionCallbacks& callbacks,std::string& message) {
        auto pairs=callbacks.entityPairs;callbacks={};const auto before=document;
        if(!DuplicateHierarchyScriptAttachments(document,target,pairs,message))return false;
        const auto after=document;callbacks.undo=[&,before]{document=before;};
        callbacks.redo=[&,after]{document=after;return true;};return true;
    });
    CommandService history;
    auto duplicate=std::make_unique<DuplicateEntityCommand>(scene,root);auto* duplicated=duplicate.get();
    Check(history.Execute(std::move(duplicate)),"duplicate a complete scripted hierarchy");
    const auto copy=duplicated->DuplicatedEntity();
    const auto copyLamp=NamedChild(scene,copy,"Lamp"),copyEffect=NamedChild(scene,copy,"Effect"),copyController=NamedChild(scene,copy,"Controller");
    const auto* copied=Owned(document,PersistentEntityId(scene,copyController));
    Check(document.attachments.size()==4&&copied&&copied->scriptInstanceId!=row.scriptInstanceId,
        "all child attachments receive fresh independent script instance IDs");
    Check(Reference(*copied,"lamp")==PersistentEntityId(scene,copyLamp)&&Reference(*copied,"effect")==PersistentEntityId(scene,copyEffect),
        "internal entity references target the copied components");
    Check(Reference(*copied,"external")==PersistentEntityId(scene,external),"ordinary duplication retains explicit external references");
    const auto copyControllerId=PersistentEntityId(scene,copyController),copyScriptId=copied->scriptInstanceId;
    Check(history.Execute(MakeDeleteSceneHierarchyCommand(scene,&document,copy))&&document.attachments.size()==2,
        "deleting the assembly removes every owned child attachment");
    Check(history.Undo()&&document.attachments.size()==4&&ValidateScriptDocumentAgainstScene(document,scene,error),
        "delete Undo restores behaviour and component owners together");
    Check(history.Undo()&&document.attachments.size()==2,"duplicate Undo removes all copied behaviours");
    Check(history.Redo()&&Owned(document,copyControllerId)&&Owned(document,copyControllerId)->scriptInstanceId==copyScriptId,
        "duplicate Redo restores the same instance identities");
    Check(history.Redo()&&document.attachments.size()==2,"delete Redo removes copied behaviours again");
    Check(history.Undo()&&history.Undo()&&document.attachments.size()==2,"return to the source assembly");
    ClearDuplicateEntityCompanionFactory();
    const auto scratch=fs::temp_directory_path()/("prefab-script-core-"+GenerateStableId());
    fs::create_directories(scratch/"Content/Scripts/Prefab");{std::ofstream f(scratch/"Content/Scripts/Prefab/Core.lua");f<<"return {}\n";}
    wi::scene::Scene payload;SceneEntityPairs pairs;
    for(auto e:{root,lamp,effect,control})pairs.emplace_back(e,payload.Entity_CreateTransform("Payload component"));
    const auto payloadRoot=pairs.front().second;
    Check(!CaptureSceneAssemblyScripts(scratch.generic_u8string(),project,scene,&document,pairs,payload,payloadRoot,error)&&
        error.find("outside the selection")!=std::string::npos&&!HasSceneAssemblyScriptTemplate(payload,payloadRoot),
        "prefab save rejects an external target before publishing a partial template");
    auto* live=FindScriptAttachment(document,row.scriptInstanceId);
    for(auto& property:live->properties)if(property.name=="external")property.referenceId.clear();
    Check(CaptureSceneAssemblyScripts(scratch.generic_u8string(),project,scene,&document,pairs,payload,payloadRoot,error),
        "capture complete attachments once external bindings are resolved: "+error);
    Check(HasSceneAssemblyScriptTemplate(payload,payloadRoot),"payload contains its versioned script templates");
    auto rootBehaviour=CreateScriptAttachment(ScriptScope::Entity,PersistentEntityId(scene,root),source);
    Check(AddScriptAttachment(document,rootBehaviour,error),"attach a behaviour to the group root itself");
    SceneAssemblyCommand unpack(scene,root,true);
    Check(unpack.Execute()&&scene.transforms.Contains(root)&&!IsSceneAssembly(scene,root)&&unpack.Pieces().size()==4,
        "Unpack keeps a behaviour-owning root as an independent authored object");
    Check(ValidateScriptDocumentAgainstScene(document,scene,error)&&scene.lights.GetComponent(lamp)->intensity==17,
        "Unpack retains live scripts and native component settings");
    for(auto e:unpack.Pieces())Check(!scene.hierarchy.Contains(e),"unpacked authored objects are independently selectable");
    unpack.Undo();Check(IsSceneAssembly(scene,root)&&scene.hierarchy.GetComponent(control)->parentID==root,
        "Unpack Undo restores the complete hierarchy and group metadata");
    Check(unpack.Execute()&&ValidateScriptDocumentAgainstScene(document,scene,error),"Unpack Redo retains script identities");
    fs::remove_all(scratch);
    std::cout<<"SCENE PREFAB SCRIPT CORE PASSED"<<std::endl;return 0;
}
