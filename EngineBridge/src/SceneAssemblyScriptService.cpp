#include "renegade/bridge/SceneAssemblyScriptService.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
namespace renegade::bridge {
namespace {
using wi::ecs::Entity;
std::vector<Entity> Hierarchy(const wi::scene::Scene& scene,Entity root) {
    wi::vector<Entity> descendants;scene.GatherChildren(root,descendants);
    std::vector<Entity> result={root};result.insert(result.end(),descendants.begin(),descendants.end());return result;
}
bool CheckSources(const std::string& root,const std::vector<ScriptAttachment>& rows,std::string& error) {
    for(const auto& row:rows) {
        if(!ValidateProjectScriptSource(root,row.sourcePath,error))return false;
        for(const auto& dependency:row.dependencies)
            if(dependency.kind==ScriptDependencyKind::ScriptModule&&!dependency.optional&&
                !ValidateProjectScriptSource(root,dependency.pathHint,error))return false;
    }return true;
}
class DeleteHierarchy final:public ICommand {
public:
    DeleteHierarchy(wi::scene::Scene& scene,ScriptDocument& scripts,Entity root)
        :scene_(scene),scripts_(scripts),root_(root),native_(scene,root){}
    bool Execute() override {
        if(!captured_) {
            before_=scripts_;after_=before_;std::unordered_set<StableId> owners;
            for(auto e:Hierarchy(scene_,root_)) {
                auto id=PersistentEntityId(scene_,e);if(IsValidStableId(id))owners.insert(id);
            }
            auto& rows=after_.attachments;
            rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row) {
                return row.scope==ScriptScope::Entity&&owners.count(row.ownerEntityId)!=0;
            }),rows.end());
            NormalizeScriptAttachmentOrder(after_);captured_=true;
        }
        if(!native_.Execute())return false;
        scripts_=after_;return true;
    }
    void Undo() override {native_.Undo();scripts_=before_;}
private:
    wi::scene::Scene& scene_;ScriptDocument& scripts_;Entity root_;
    DeleteEntityCommand native_;ScriptDocument before_,after_;bool captured_=false;
};
}
bool CaptureSceneAssemblyScripts(const std::string& projectRoot,const StableId& project,
    const wi::scene::Scene& source,const ScriptDocument* document,
    const SceneEntityPairs& pairs,wi::scene::Scene& payload,Entity payloadRoot,std::string& error) {
    error.clear();if(!document)return true;
    if(document->envelope.projectId!=project||!ValidateScriptDocumentAgainstScene(*document,source,error)) {
        if(error.empty())error="The prefab scripting document belongs to a different project.";return false;
    }
    std::unordered_map<StableId,Entity> selected;
    for(const auto& pair:pairs) {
        const auto id=PersistentEntityId(source,pair.first);
        if(IsValidStableId(id)&&!selected.emplace(id,pair.second).second) {
            error="Two selected components share a script identity. Save and repair the scene before saving this prefab.";return false;
        }
    }
    std::vector<ScriptAttachment> rows;
    for(const auto& row:document->attachments)
        if(row.scope==ScriptScope::Entity&&selected.count(row.ownerEntityId))rows.push_back(row);
    if(rows.empty())return true;
    for(const auto& row:rows)for(const auto& property:row.properties)
        if(property.type==ScriptPropertyType::EntityReference&&!property.referenceId.empty()&&
            !selected.count(property.referenceId)) {
            error="A prefab script targets an object outside the selection. Include that object or clear the reference before saving.";return false;
        }
    if(!CheckSources(projectRoot,rows,error))return false;
    std::string text;if(!SerializeScriptAttachmentTemplates(project,rows,text,error))return false;
    for(const auto& item:selected) {
        auto* m=payload.metadatas.GetComponent(item.second);if(!m)m=&payload.metadatas.Create(item.second);
        m->string_values.set(SceneAssemblyLocalIdKey,item.first);
    }
    auto* m=payload.metadatas.GetComponent(payloadRoot);if(!m)m=&payload.metadatas.Create(payloadRoot);
    m->string_values.set(SceneAssemblyScriptTemplateKey,text);
    for(const auto& row:rows)if(selected.at(row.ownerEntityId)==payloadRoot)
        m->bool_values.set(SceneAssemblyAuthoredRootKey,true);
    return true;
}
bool HasSceneAssemblyScriptTemplate(const wi::scene::Scene& scene,Entity root) {
    for(auto e:Hierarchy(scene,root))if(const auto* m=scene.metadatas.GetComponent(e);
        m&&m->string_values.has(SceneAssemblyScriptTemplateKey))return true;
    return false;
}
bool ApplySceneAssemblyScripts(wi::scene::Scene& scene,Entity root,const std::string& projectRoot,
    ScriptDocument* document,ReusablePlacementCompanionCallbacks& callbacks,std::string& error) {
    callbacks={};error.clear();const auto members=Hierarchy(scene,root);
    std::vector<Entity> templates;
    for(auto e:members)if(const auto* m=scene.metadatas.GetComponent(e);
        m&&m->string_values.has(SceneAssemblyScriptTemplateKey))templates.push_back(e);
    if(templates.empty())return true;
    if(!document){error="Save the Level before placing a prefab with scripts.";return false;}
    if(templates.size()!=1){error="Prefab contains conflicting scripting templates. Save it again from its source objects.";return false;}
    std::vector<ScriptAttachment> rows;
    const auto text=scene.metadatas.GetComponent(templates.front())->string_values.get(SceneAssemblyScriptTemplateKey);
    if(!DeserializeScriptAttachmentTemplates(text,document->envelope.projectId,rows,error)||
        !CheckSources(projectRoot,rows,error))return false;
    std::unordered_map<StableId,StableId> ids;
    for(auto e:members)if(const auto* m=scene.metadatas.GetComponent(e);m&&m->string_values.has(SceneAssemblyLocalIdKey)) {
        const auto local=m->string_values.get(SceneAssemblyLocalIdKey),live=PersistentEntityId(scene,e);
        if(!IsValidStableId(local)||!IsValidStableId(live)||!ids.emplace(local,live).second) {
            error="Prefab component identities could not be mapped safely.";return false;
        }
    }
    auto candidate=*document;
    for(auto row:rows) {
        const auto owner=ids.find(row.ownerEntityId);
        if(owner==ids.end()){error="Prefab script owner is missing.";return false;}
        row.ownerEntityId=owner->second;row.scriptInstanceId=GenerateStableId();
        for(auto& property:row.properties)
            if(property.type==ScriptPropertyType::EntityReference&&!property.referenceId.empty()) {
                const auto target=ids.find(property.referenceId);
                if(target==ids.end()){error="Prefab script target is missing.";return false;}
                property.referenceId=target->second;property.pathHint.clear();
            }
        candidate.attachments.push_back(std::move(row));
    }
    NormalizeScriptAttachmentOrder(candidate);
    if(!ValidateScriptDocumentAgainstScene(candidate,scene,error))return false;
    const auto before=*document;*document=candidate;
    callbacks.undo=[document,before]{*document=before;};
    callbacks.redo=[document,candidate]{*document=candidate;return true;};
    for(auto e:members)if(auto* m=scene.metadatas.GetComponent(e)) {
        m->string_values.erase(SceneAssemblyScriptTemplateKey);m->string_values.erase(SceneAssemblyLocalIdKey);
    }
    return true;
}
bool DuplicateHierarchyScriptAttachments(ScriptDocument& document,const wi::scene::Scene& scene,
    const SceneEntityPairs& pairs,std::string& error) {
    std::unordered_map<StableId,StableId> ids;
    for(const auto& pair:pairs) {
        auto source=PersistentEntityId(scene,pair.first),target=PersistentEntityId(scene,pair.second);
        if(!source.empty()&&!IsValidStableId(source)) {
            error="A source component has malformed persistent identity.";return false;
        }
        if(IsValidStableId(source)) {
            if(!IsValidStableId(target)||source==target||!ids.emplace(source,target).second) {
                error="Duplicated component identities could not be mapped safely.";return false;
            }
        }
    }
    auto candidate=document;
    for(const auto& original:document.attachments) {
        const auto owner=ids.find(original.ownerEntityId);
        if(original.scope!=ScriptScope::Entity||owner==ids.end())continue;
        auto row=original;row.ownerEntityId=owner->second;row.scriptInstanceId=GenerateStableId();
        for(auto& property:row.properties)if(property.type==ScriptPropertyType::EntityReference) {
            const auto target=ids.find(property.referenceId);
            if(target!=ids.end()){property.referenceId=target->second;property.pathHint.clear();}
        }
        candidate.attachments.push_back(std::move(row));
    }
    NormalizeScriptAttachmentOrder(candidate);
    if(!ValidateScriptDocumentAgainstScene(candidate,scene,error))return false;
    document=std::move(candidate);error.clear();return true;
}
std::unique_ptr<ICommand> MakeDeleteSceneHierarchyCommand(wi::scene::Scene& scene,ScriptDocument* scripts,Entity root) {
    if(scripts)return std::make_unique<DeleteHierarchy>(scene,*scripts,root);
    return std::make_unique<DeleteEntityCommand>(scene,root);
}
}
