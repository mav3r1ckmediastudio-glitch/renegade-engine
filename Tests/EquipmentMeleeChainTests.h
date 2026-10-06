#pragma once
#include <limits>
static void TestEquipmentMeleeChains(renegade::bridge::EquipmentDefinition sword,
 renegade::bridge::EquipmentDefinition shield) {
 using namespace renegade::bridge;
 const auto make=[&]() {
  renegade::runtime::RuntimeEquipmentLoadout r;r.authored=r.ready=true;
  r.primary.equipment=sword;r.offHand.equipment=shield;
  r.primary.equipment.actions={{EquipmentAction::Charge,"Charge",0,0,0,0,true,true},
   {EquipmentAction::Release,"Release",0,0,1,.1f,false,true}};
  return r;
 };
 GameplayInputFrame press;press.firePressed=press.fireDown=true;
 press.offHandUsePressed=press.offHandUseDown=true;
 GameplayInputFrame block;block.offHandUseDown=true;
 auto r=make();
 const auto route=[&](GameplayInputFrame input,bool busy,float dt,bool window=true) {
  return r.RouteStaged(input,true,busy,dt,false,true,true,true,window,0);
 };
 route(press,false,.01f);
 Check(r.chargePresentation && r.offHandBlockPresentation,"chain initial charge/block");
 route(block,false,.01f);
 Check(r.releasePresentation,"chain initial release");
 auto next=press;next.offHandUsePressed=false;next.player.lookYaw=.1f;
 route(next,true,.02f,false);
 Check(!r.queuedStrike.pending,"early strike press entered queue");
 route(next,true,.02f);
 Check(r.queuedStrike.pending && r.queuedStrike.direction==1 && !r.releasePresentation,"queue direction/ownership");
 const auto seconds=r.queuedStrike.seconds;
 route(block,true,0);
 Check(r.queuedStrike.seconds==seconds && !r.queuedStrike.released,"pause changed queue");
 route(block,true,.02f);
 Check(r.queuedStrike.released,"queue release lost");
 route(block,false,.1f);
 Check(!r.chainedChargeStarted && r.actions.ReservedHands()==2,"queued strike bypassed recovery");
 route(block,false,.01f);
 Check(r.chainedChargeStarted && r.chainedDirection==1 && r.chainedCount==1 &&
       std::abs(r.chainedSeconds-.04f)<.0001f && r.releasePresentation &&
       r.offHandBlockPresentation && r.actions.ReservedHands()==3,"queued dispatch payload/shield");
 route(block,true,.01f);
 Check(!r.chainedChargeStarted && !r.releasePresentation && r.chainedCount==1,"queue replayed");
 route(next,true,.02f);
 auto cancel=block;cancel.cancelEquipmentPressed=true;route(cancel,true,.01f);
 Check(!r.queuedStrike.pending && r.actions.ReservedHands()==3,"cancel interrupted active or kept queue");
 route(next,true,.02f);route(block,true,.02f);route(block,true,.76f);
 Check(!r.queuedStrike.pending,"stale released queue retained");
 route(next,true,.02f);
 auto held=block;held.fireDown=true;route(held,true,1);
 Check(r.queuedStrike.pending && r.queuedStrike.seconds==1,"held buffer charge cap");
 route(held,false,.1f);route(held,false,.01f);
 Check(r.chainedChargeStarted && r.chargePresentation && !r.releasePresentation &&
       r.chainedSeconds==1,"held chain auto released");
 route(block,false,.01f);
 Check(r.releasePresentation,"held chain release missing");
 route(next,true,.02f);
 auto unequip=block;unequip.toggleEquipmentPressed=true;route(unequip,true,.01f);
 Check(!r.queuedStrike.pending,"equipment change retained queue");
 r.RouteStaged(next,true,true,.01f,false,true,true,false,true);
 Check(!r.queuedStrike.pending,"non-directional assembly gained buffer");
 r=make();route(press,false,.01f);route(block,false,.01f);
 r.primary.equipment.actions[0].prepareSeconds=.1f;
 r.primary.equipment.actions[0].windupSeconds=.1f;
 route(next,true,.02f);route(block,true,.02f);route(block,false,.1f);route(block,false,.01f);
 Check(r.chainedChargeStarted && r.chainedReleasedCharge && r.chargePresentation &&
       !r.releasePresentation,"released queue preparation/strength pin");
 route(block,false,.02f);
 Check(r.chainedReleasedCharge && std::abs(r.chainedSeconds-.04f)<.0001f,"queued strength lost during preparation");
 route(cancel,false,.01f);
 Check(!r.chainedReleasedCharge && !r.queuedStrike.pending,"cancel retained queued strength");
 r=make();route(press,false,.01f);route(block,false,.01f);
 route(next,false,.01f,false);
 Check(r.queuedStrike.pending && r.chainInputWindow,"first recovery tick lost press");
 auto before=r.queuedStrike.seconds;
 route(block,false,std::numeric_limits<float>::quiet_NaN());
 Check(r.queuedStrike.pending && r.queuedStrike.seconds==before,"nonfinite time changed queue");
 r.primary.equipment.assetId=GenerateStableId();route(block,false,.01f);
 Check(!r.queuedStrike.pending,"changed equipment retained queue");

}
