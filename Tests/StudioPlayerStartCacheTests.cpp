#include "../Studio/src/PlayerStartFrameCache.h"
#include <iostream>
#include <cstdlib>
using namespace renegade;
static void Check(bool value,const char* message){
    if(!value){std::cerr<<message<<"\n";std::exit(1);}
}
int main(){
    studio::PlayerStartFrameCache cache;
    wi::scene::Scene scene;
    using R=bridge::PlayerStartResolution;
    Check(cache.Resolve(scene,1,1,0).resolution==R::Missing,"empty scene");
    bridge::CreatePlayerStartCommand create(scene,{});
    Check(create.Execute(),"create marker");
    auto first=cache.Resolve(scene,1,1,0);
    Check(first.resolution==R::Success,"metadata creation invalidates same frame");
    const auto entity=first.start.entity;
    auto* transform=scene.transforms.GetComponent(entity);
    transform->translation_local.x=7;transform->SetDirty();transform->UpdateTransform();
    Check(cache.Resolve(scene,1,1,0).start.transform.translation.x==7,"live drag must not cache pose");
    auto settings=first.start.settings;settings.walkSpeed=9;
    bridge::SetPlayerControllerSettingsCommand edit(scene,entity,settings);
    Check(edit.Execute(),"edit player settings");
    Check(cache.Resolve(scene,1,1,1).start.settings.walkSpeed==9,"command revision invalidates settings");
    edit.Undo();
    Check(cache.Resolve(scene,1,1,2).start.settings.walkSpeed==first.start.settings.walkSpeed,"Undo settings");
    const auto duplicate=scene.Entity_CreateTransform("Duplicate marker");
    scene.metadatas.Create(duplicate).string_values.set(bridge::PlayerStartMetadataKey,bridge::PlayerStartMetadataVersion);
    Check(cache.Resolve(scene,1,1,2).resolution==R::Multiple,"duplicate marker validation");
    scene.Entity_Remove(duplicate);
    Check(cache.Resolve(scene,1,1,2).resolution==R::Success,"delete duplicate");
    scene.metadatas.GetComponent(entity)->string_values.set(bridge::PlayerStartMetadataKey,"disabled");
    Check(cache.Resolve(scene,1,1,2).resolution==R::Missing,"removed metadata tag not stale");
    scene.metadatas.GetComponent(entity)->string_values.set(bridge::PlayerStartMetadataKey,bridge::PlayerStartMetadataVersion);
    Check(cache.Resolve(scene,2,1,2).resolution==R::Success,"next frame refreshes direct edits");
    wi::scene::Scene other;
    Check(cache.Resolve(other,2,1,2).resolution==R::Missing,"scene pointer isolation");
    Check(cache.Resolve(scene,2,2,2).start.entity==entity,"scene revision reload");
    create.Undo();
    Check(cache.Resolve(scene,2,2,3).resolution==R::Missing,"Undo creation");
    Check(create.Execute(),"Redo creation");
    Check(cache.Resolve(scene,2,2,4).resolution==R::Success,"Redo validation");
    std::cout<<"Player Start frame-cache invalidation and live pose PASS\n";
}
