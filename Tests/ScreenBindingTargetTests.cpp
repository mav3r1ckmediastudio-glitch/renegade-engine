#include "renegade/bridge/ScreenBindingTargetService.h"
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    wi::scene::Scene scene;
    const auto actor=[&](const std::string& name,const StableId& id){
        const auto entity=wi::ecs::CreateEntity();scene.names.Create(entity).name=name;
        auto& metadata=scene.metadatas.Create(entity);
        metadata.string_values.set(CharacterMetadataKey,"1");
        metadata.int_values.set(CharacterSchemaVersionMetadataKey,CharacterSchemaVersion);
        if(!id.empty())metadata.string_values.set(PersistentEntityIdMetadataKey,id);
        return entity;
    };
    const auto aId=GenerateStableId(),bId=GenerateStableId(),duplicateId=GenerateStableId();
    const auto a=actor("Alpha",aId);actor("Beta",bId);actor("Missing",{});
    actor("Duplicate A",duplicateId);actor("Duplicate B",duplicateId);
    const auto nonCharacter=wi::ecs::CreateEntity();scene.names.Create(nonCharacter).name="Player named prop";
    const auto before=scene.metadatas.GetCount();
    auto catalogue=CollectScreenBindingActors(scene);
    check(catalogue.characters.size()==2 && catalogue.characters[0].id==aId &&
        catalogue.characters[1].id==bId,"Catalogue fabricated targets or retained duplicate/missing identities");
    check(catalogue.diagnostics.size()==2 && scene.metadatas.GetCount()==before,
        "Catalogue failed diagnostics or modified scene metadata");
    scene.names.GetComponent(a)->name="Zulu";catalogue=CollectScreenBindingActors(scene);
    check(catalogue.characters.size()==2 && catalogue.characters[0].id==bId &&
        catalogue.characters[1].id==aId && catalogue.characters[1].name=="Zulu","Character rename changed identity or stale ordering");
    scene.Entity_Remove(a);catalogue=CollectScreenBindingActors(scene);
    check(catalogue.characters.size()==1 && catalogue.characters[0].id==bId,"Deleted Character remained selectable");
    if(failures)return 1;
    std::cout<<"PASS: real Character catalogue, stable rename identity, deletion, duplicate diagnostics and no scene mutation\n";
}
