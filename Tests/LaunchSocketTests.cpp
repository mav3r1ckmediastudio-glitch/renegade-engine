#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "RuntimeProjectileAim.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include <iostream>
#include <limits>
using namespace renegade;
int main() {
 std::cerr<<"initializing jobs\n";
 wi::jobsystem::Initialize();
 std::cerr<<"jobs ready\n";
 const auto update=[](wi::scene::Scene& native) {
  wi::jobsystem::context ctx;
  native.RunTransformUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
  native.RunHierarchyUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
 };
 const auto fail=[](const char* m){std::cerr<<m;return 1;};
 std::string text,error,again;
 bridge::LaunchSocketDefinition muzzle;
 muzzle.name="Muzzle";muzzle.parentPath="[\"Gun\",\"FIRESPOT\"]";muzzle.position={.1f,0,.5f};muzzle.rotationDegrees={0,90,0};
 bridge::FirstPersonAssemblySettings settings;
 settings.armsAssetId=bridge::GenerateStableId();settings.weaponAssetId=bridge::GenerateStableId();
 settings.parentBonePath="[\"Arms\",\"Hand\"]";settings.pairs={{"Idle",0,0}};
 settings.launchSockets={muzzle};
 bridge::FirstPersonAssemblySettings parsed;
 if(!bridge::SerializeFirstPersonAssemblySettings(settings,text,error)||
    !bridge::ParseFirstPersonAssemblySettings(text,parsed,error)||
    !bridge::SerializeFirstPersonAssemblySettings(parsed,again,error)||text!=again)
  return fail("socket recipe roundtrip");
 auto bad=settings;bad.launchSockets.push_back(muzzle);
 if(bridge::SerializeFirstPersonAssemblySettings(bad,again,error))return fail("duplicate names admitted");
 bad=settings;bad.launchSockets[0].rotationDegrees.x=std::numeric_limits<float>::quiet_NaN();
 if(bridge::SerializeFirstPersonAssemblySettings(bad,again,error))return fail("nonfinite direction admitted");
 // Explicit native parent survives archive ID remapping and animated hierarchy propagation.
 wi::scene::Scene scene;
 const auto root=scene.Entity_CreateTransform("Gun"),bone=scene.Entity_CreateTransform("FIRESPOT");
 scene.Component_Attach(bone,root,true);scene.transforms.GetComponent(bone)->Translate(XMFLOAT3(0,0,2));
 update(scene);
 wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
 wi::scene::Scene copy;copy.Serialize(archive);
 if(!bridge::AttachLaunchSockets(copy,settings.launchSockets,bridge::LaunchSocketPart::PrimaryWeapon,error))
  return fail("explicit parent attach after remap");
 update(copy);
 const auto parent=bridge::ResolveLaunchSocketParent(copy,muzzle.parentPath);
 const auto copyRoot=bridge::ResolveLaunchSocketParent(copy,"");
 XMFLOAT3 p,d;
 if(!bridge::ReadLaunchSocketPose(copy,copyRoot,"Muzzle",p,d,error)||
    std::abs(p.z-2.5f)>.0001f||std::abs(d.x-1)>.0001f)return fail("socket pose");
 copy.transforms.GetComponent(parent)->Translate(XMFLOAT3(0,1,0));update(copy);
 if(!bridge::ReadLaunchSocketPose(copy,copyRoot,"Muzzle",p,d,error)||std::abs(p.y-1)>.0001f)
  return fail("animated parent follow");
 wi::Archive saved;copy.Serialize(saved);saved.SetReadModeAndResetPos(true);
 wi::scene::Scene restored;restored.Serialize(saved);update(restored);
 if(!bridge::ReadLaunchSocketPose(restored,bridge::ResolveLaunchSocketParent(restored,""),"Muzzle",p,d,error))
  return fail("native socket save reload");
 bridge::ProjectileAssetDocument projectile;
 projectile.projectId=bridge::GenerateStableId();projectile.assetId=bridge::GenerateStableId();projectile.name="Bullet";
 bridge::ProjectileSource source;source.ownerSubjectId="Player";
 XMFLOAT3 origin,dir;unsigned calls=0;
 const auto clear=[&](const auto&,const auto&,const auto&) {++calls;return bridge::ProjectileQueryResult{};};
 if(!runtime::ResolveProjectileMuzzleAim(projectile,source,{0,0,0},{0,0,1},{.2f,-.2f,.6f},
    {0,0,1},clear,origin,dir,error)||calls!=2||origin.z!=.6f||dir.z<.99f)
  return fail("camera muzzle convergence");
 const auto cover=[](const auto&,const auto&,const auto&) {
  bridge::ProjectileQueryResult r;r.status=bridge::ProjectileQueryStatus::Hit;r.contact.position={0,0,.3f};return r;
 };
 if(!runtime::ResolveProjectileMuzzleAim(projectile,source,{0,0,0},{0,0,1},{0,0,.6f},
    {0,0,1},cover,origin,dir,error)||origin.z>=.3f||dir.z<.99f)return fail("cover clipped muzzle");
 if(runtime::ResolveProjectileMuzzleAim(projectile,source,{0,0,0},{0,0,1},{0,0,.6f},
    {0,0,-1},clear,origin,dir,error))return fail("backwards socket admitted");
 const auto blocked=[](const auto&,const auto&,const auto&) {
  bridge::ProjectileQueryResult r;r.status=bridge::ProjectileQueryStatus::Blocked;return r;
 };
 if(runtime::ResolveProjectileMuzzleAim(projectile,source,{0,0,0},{0,0,1},{0,0,.6f},
    {0,0,1},blocked,origin,dir,error))return fail("blocked query admitted");
 // Input/output aliasing is supported by the Runtime call site.
 origin={0,0,0};dir={0,0,1};
 if(!runtime::ResolveProjectileMuzzleAim(projectile,source,origin,dir,{0,0,.6f},
    {0,0,1},cover,origin,dir,error)||origin.z>=.3f)return fail("aliased cover query");
 bridge::EquipmentAssetDocument equipment,loaded;
 equipment.projectId=projectile.projectId;equipment.equipment.assetId=bridge::GenerateStableId();
 equipment.equipment.name="Socket weapon";equipment.equipment.presentationAssetId=bridge::GenerateStableId();
 equipment.equipment.actions={{bridge::EquipmentAction::PrimaryUse,"Attack"}};
 equipment.equipment.projectiles={{bridge::EquipmentAction::PrimaryUse,projectile.assetId,"Muzzle"}};
 if(!bridge::SerializeEquipmentAsset(equipment,text,error)||
    !bridge::DeserializeEquipmentAsset(text,loaded,error)||
    loaded.equipment.projectiles[0].launchSocketName!="Muzzle")return fail("equipment socket binding roundtrip");
 std::cout<<"Launch sockets: recipe reopen, archive remap, animated parent, native reload, muzzle aim and cover PASS\n";
}
