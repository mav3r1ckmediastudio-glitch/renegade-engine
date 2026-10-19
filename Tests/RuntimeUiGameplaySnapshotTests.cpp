#include "../Runtime/src/RuntimeUiGameplaySnapshot.h"
#include <iostream>
using namespace renegade::bridge;
using namespace renegade::runtime;
int main()
{
    int failures=0;
    const auto check=[&](bool valid,const char* message) {
        if (!valid) { ++failures; std::cerr<<message<<'\n'; }
    };
    RuntimePlayerState player;
    wi::scene::Scene scene;
    std::string error;
    CreatePlayerStartCommand start(scene,TransformState{});
    check(start.Execute(),"Production Player Start creation failed");
    const auto playerStart=ResolvePlayerStart(scene);
    check(playerStart.resolution==PlayerStartResolution::Success &&
        SpawnRuntimePlayer(scene,playerStart.start,player,error),"Production Player spawn failed");
    RuntimeCombatState combat;
    combat.playerHealth=75;combat.playerMaxHealth=120;
    RuntimePlayerViewAnimationState view;
    view.initialized=true;view.pairedAssembly=true;
    view.loadedShells=1;view.firearm.capacity=3;
    const auto characterId=GenerateStableId();
    RuntimeCharacterSystemState characters;
    RuntimeCharacterRecord actor;
    actor.stableEntityId=characterId;actor.entity=wi::ecs::CreateEntity();
    scene.characters.Create(actor.entity);
    characters.characters={actor};
    CharacterCombatRecord character;
    character.characterId=characterId;character.entity=actor.entity;
    character.health=65;character.maxHealth=100;
    character.weapon.magazineSize=12;character.magazineAmmo=4;character.reserveAmmo=20;
    combat.characters={character};
    GameplayAttributeProfile profile{GenerateStableId(),"Custom",
        {{"energy","Energy",60.0,0.0,100.0,0.0},
         {"health.current","Custom health",999.0,0.0,1000.0,0.0}}};
    GameplayAttributeInstance custom;
    check(custom.ApplyProfile(profile,{},true,error),"Custom gameplay profile fixture failed");
    std::map<StableId,GameplayAttributeInstance> customCharacters{{characterId,custom}};
    RuntimeUiGameplaySources sources;
    sources.player=&player;sources.scene=&scene;sources.combat=&combat;
    sources.characters=&characters;sources.playerView=&view;
    sources.combatReady=true;sources.sceneGeneration=10;
    sources.playerAttributes=&custom;sources.characterAttributes=&customCharacters;
    sources.projectAttributes=&custom;
    std::vector<ScreenBindingSource> requests={
        {ScreenBindingScope::PossessedPlayer,{},"health.current"},
        {ScreenBindingScope::PossessedPlayer,{},"health.maximum"},
        {ScreenBindingScope::PossessedPlayer,{},"ammo.loaded"},
        {ScreenBindingScope::PossessedPlayer,{},"ammo.capacity"},
        {ScreenBindingScope::PossessedPlayer,{},"ammo.reserve"},
        {ScreenBindingScope::PossessedPlayer,{},"energy"},
        {ScreenBindingScope::Character,characterId,"health.current"},
        {ScreenBindingScope::Character,characterId,"ammo.loaded"},
        {ScreenBindingScope::Character,characterId,"ammo.reserve"},
        {ScreenBindingScope::Character,characterId,"energy"},
        {ScreenBindingScope::ProjectAttribute,{},"energy"}};
    RuntimeUiSnapshotPublisher publisher;
    ScreenBindingSnapshot snapshot;
    ScreenBindingContext context;
    check(!publisher.BeginSession(0,error) && publisher.BeginSession(10,error),"Session admission failed");
    check(publisher.Publish(requests,1,sources,snapshot,context,error),"Authoritative snapshot failed");
    const auto find=[&](const ScreenBindingSource& source)->const ScreenBindingSample* {
        for (const auto& sample:snapshot.values) if (sample.source==source) return &sample;
        return nullptr;
    };
    const auto number=[&](std::size_t index,double expected,const char* message) {
        const auto* sample=find(requests[index]);
        check(sample && std::holds_alternative<double>(sample->value) &&
            std::get<double>(sample->value)==expected,message);
    };
    number(0,75,"Player health did not come from combat");
    number(1,120,"Player maximum did not come from combat");
    number(2,1,"Loaded ammunition did not come from native animation state");
    number(3,3,"Capacity did not come from native firearm settings");
    check(!find(requests[4]),"Unmodelled player reserve ammo was fabricated");
    number(5,60,"Custom player attributes did not remain gameplay-owned");
    number(10,60,"Independent Project attribute was not published");
    number(6,65,"Character health did not come from combat");
    number(7,4,"Character loaded ammo did not come from combat");
    number(8,20,"Character reserve ammo did not come from combat");
    const auto oldSnapshot=snapshot;
    const auto oldGeneration=context.Generation(requests[0]);
    check(!publisher.Publish(requests,1,sources,snapshot,context,error),
        "A duplicate gameplay revision was published");
    combat.playerHealth=40;view.loadedShells=0;
    check(publisher.Publish(requests,2,sources,snapshot,context,error),"Changed gameplay snapshot failed");
    number(0,40,"Native damage was not observed");
    number(2,0,"Accepted ammunition consumption was not observed");
    check(combat.playerHealth==40 && view.loadedShells==0,"UI publisher modified gameplay authority");

    const auto oldEntity=player.entity;
    scene.rigidbodies.Remove(oldEntity);
    check(publisher.Publish(requests,3,sources,snapshot,context,error) &&
        !find(requests[0]) && context.Generation(requests[0])==0,
        "Despawned native player remained a valid binding target");
    player={};
    check(SpawnRuntimePlayer(scene,playerStart.start,player,error),"Production Player respawn failed");
    check(publisher.Publish(requests,4,sources,snapshot,context,error) &&
        context.Generation(requests[0])!=oldGeneration,"Respawn reused a binding generation");
    ScreenBinding label;label.source=requests[0];
    const auto stale=EvaluateScreenBinding(label,oldSnapshot,context.session,context.Generation(label.source),
        std::string("Unavailable"));
    check(stale.diagnostic==ScreenBindingDiagnostic::StaleGeneration,"Late pre-respawn values were accepted");

    sources.combatReady=false;
    check(publisher.Publish(requests,5,sources,snapshot,context,error),"Combat-independent snapshot failed");
    check(!find(requests[0]),"Custom health silently replaced unavailable native health");
    number(5,60,"Generic Player attributes depended on combat");
    number(9,60,"Generic Character attributes depended on combat");
    number(10,60,"Project attribute depended on Player or combat");
    sources.combatReady=true;characters.characters.clear();
    check(publisher.Publish(requests,6,sources,snapshot,context,error) && !find(requests[6]),
        "A stale combat record kept a despawned character binding alive");
    publisher.Stop();
    check(!publisher.BeginSession(10,error) && publisher.BeginSession(11,error),
        "A stopped Runtime session identity was reused");
    check(publisher.Publish(requests,1,sources,snapshot,context,error),"New-session initial snapshot failed");
    check(EvaluateScreenBinding(label,oldSnapshot,context.session,context.Generation(label.source),
        std::string("Unavailable")).diagnostic==ScreenBindingDiagnostic::StaleSession,
        "A previous Runtime session supplied live UI data");
    if (failures) return 1;
    std::cout<<"PASS: authoritative health/ammo adapters, independent custom attributes and session/entity lifetimes\n";
}
