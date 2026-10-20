#pragma once
static void CheckSpellChargeDamage() {
    using namespace renegade::runtime;
    RuntimeEquipmentLoadout state;state.authored=state.ready=true;
    state.primary.equipment={GenerateStableId(),"Charge proof","",EquipmentHandUse::PrimaryOnly,
        {{EquipmentAction::Charge,"Charge",0,0,1,0,true,true},
         {EquipmentAction::Release,"Release",0,0,.2f,0,false,true}}};
    RuntimeEquipmentLoadout::ProjectileRequest request{};
    request.equipmentId=state.primary.equipment.assetId;request.action=EquipmentAction::Release;
    request.projectile.damage=30;request.projectile.minimumChargeDamage=10;request.projectile.fullChargeSeconds=2.5f;
    request.projectile.burning={5,4};request.projectile.minimumChargeBurning={2,2};
    state.resolvedProjectiles.push_back(request);
    GameplayInputFrame held;held.firePressed=held.fireDown=true;
    state.RouteStaged(held,true,false,.25f,false,true);
    held.firePressed=false;
    state.RouteStaged(held,true,false,1.f,false,true);
    const float saved=state.projectileChargeSeconds;
    state.RouteStaged(held,true,false,0,false,true);
    Check(state.projectileChargeSeconds==saved,"paused charge power advanced");
    GameplayInputFrame release;
    state.RouteStaged(release,true,false,.01f,false,true);
    auto shots=state.TakeProjectileRequests();
    Check(shots.size()==1 && std::abs(shots[0].projectile.damage-20.f)<.001f && shots[0].projectile.burning.damagePerSecond==3.5f && shots[0].projectile.burning.durationSeconds==3,"release did not snapshot half charge damage");
    state.ScheduleProjectile(shots[0],123);
    state.projectileChargeSeconds=2.5f;
    auto due=state.ReleaseProjectiles(123,1,false);
    Check(due.size()==1 && due[0].projectile.damage==20,"animation delay changed accepted payload");
    state.actions.Reset();state.dispatched=false;
    held.firePressed=true;state.RouteStaged(held,true,false,.1f,false,true);
    held.firePressed=false;state.RouteStaged(held,true,false,4.f,false,true);
    Check(state.projectileChargeSeconds==2.5f,"charge power not capped");
    auto cancel=held;cancel.cancelEquipmentPressed=true;
    state.RouteStaged(cancel,true,false,.1f,false,true);
    Check(state.projectileChargeSeconds==0 && state.TakeProjectileRequests().empty(),"cancel retained power or launched");
}
