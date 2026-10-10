#include "renegade/bridge/EquipmentActionState.h"
#include <iostream>
#include <limits>
using namespace renegade::bridge;
int main() {
 EquipmentDefinition sword{"sword","Sword","",EquipmentHandUse::PrimaryOnly,
  {{EquipmentAction::PrimaryUse,"Attack",0.1f,0.2f,0.3f,0.4f,false,true}}};
 EquipmentDefinition shield{"shield","Shield","",EquipmentHandUse::OffHandOnly,
  {{EquipmentAction::Block,"Block",0,0,0.1f,0.1f,true,true}}};
 auto bow=sword;bow.assetId="bow";bow.handUse=EquipmentHandUse::TwoHanded;
 EquipmentActionState state;
 if(!state.Begin(sword,EquipmentAction::PrimaryUse,EquipmentHand::Primary)||
    !state.Begin(shield,EquipmentAction::Block,EquipmentHand::OffHand)||state.ReservedHands()!=3||
    state.Begin(bow,EquipmentAction::PrimaryUse,EquipmentHand::Primary))return 1;
 state.Update(0);state.Update(std::numeric_limits<float>::quiet_NaN());
 if(state.Channels()[0].phase!=EquipmentActionPhase::Prepare)return 2;
 state.Update(0.5f);
 if(state.Channels()[0].phase!=EquipmentActionPhase::Active||state.Channels()[1].phase!=EquipmentActionPhase::Hold||
    state.Cancel("sword"))return 3;
 if(!state.Release("shield"))return 4;
 state.Update(1);
 if(state.ReservedHands()!=0)return 5;
 unsigned active=0;
 for(const auto& e:state.TakeEvents())if(e.phase==EquipmentActionPhase::Active)++active;
 if(active!=2)return 6;
 if(state.Begin(sword,EquipmentAction::PrimaryUse,EquipmentHand::OffHand)||
    state.Begin(sword,EquipmentAction::Cast,EquipmentHand::Primary)||
    !state.Begin(bow,EquipmentAction::PrimaryUse,EquipmentHand::Primary)||state.ReservedHands()!=3||
    !state.Cancel("bow")||state.ReservedHands()!=0)return 7;
 auto instant=sword;instant.actions[0]={EquipmentAction::Use,"Use",0,0,0,0};
 if(!state.Begin(instant,EquipmentAction::Use,EquipmentHand::Primary))return 8;
 state.Update(10);if(state.ReservedHands()!=0)return 9;
 auto bad=sword;bad.actions[0].windupSeconds=-1;
 if(ValidateEquipmentDefinition(bad)||state.Begin(bad,EquipmentAction::PrimaryUse,EquipmentHand::Primary))return 10;
 bad=sword;bad.actions.push_back(bad.actions[0]);if(ValidateEquipmentDefinition(bad))return 11;
 if(!state.Begin(shield,EquipmentAction::Block,EquipmentHand::OffHand))return 12;
 state.Reset();if(state.ReservedHands()!=0||!state.TakeEvents().empty())return 13;
 auto support=sword;support.handUse=EquipmentHandUse::PrimaryWithSupport;
 if(!state.Begin(support,EquipmentAction::PrimaryUse,EquipmentHand::Primary)||
    state.ReservedHands()!=3||state.Begin(shield,EquipmentAction::Block,EquipmentHand::OffHand))return 14;
 state.Reset();
 auto either=sword;either.handUse=EquipmentHandUse::EitherHand;
 if(!state.Begin(either,EquipmentAction::PrimaryUse,EquipmentHand::OffHand)||
    state.ReservedHands()!=2||!state.Begin(sword,EquipmentAction::PrimaryUse,EquipmentHand::Primary))return 15;
 state.Reset();
 bad=sword;bad.handUse=static_cast<EquipmentHandUse>(99);
 if(ValidateEquipmentDefinition(bad))return 16;
 bad=sword;bad.actions[0].activeSeconds=std::numeric_limits<float>::infinity();
 if(ValidateEquipmentDefinition(bad))return 17;

 state.Reset();
 auto twin=sword;twin.handUse=EquipmentHandUse::EitherHand;twin.actions[0].holdUntilRelease=true;
 if(!state.Begin(twin,EquipmentAction::PrimaryUse,EquipmentHand::Primary) ||
    !state.Begin(twin,EquipmentAction::PrimaryUse,EquipmentHand::OffHand))return 18;
 state.Update(1,true);
 if(!state.Release(twin.assetId,EquipmentHand::Primary))return 19;
 state.Update(.01f,true);
 if(state.Channels()[0].phase!=EquipmentActionPhase::Active ||
    state.Channels()[1].phase!=EquipmentActionPhase::Hold ||
    !state.CompleteActive(twin.assetId,EquipmentHand::Primary) ||
    state.CompleteActive(twin.assetId,EquipmentHand::OffHand))return 20;
 if(!state.Cancel(twin.assetId,EquipmentHand::OffHand) || state.ReservedHands()!=1)return 21;
 state.Update(1,true);
 if(state.ReservedHands()!=0)return 22;
 state.Reset();twin.actions[0].holdUntilRelease=false;
 twin.actions.push_back({EquipmentAction::Release,"Release",0,0,.3f,.2f});
 state.Begin(twin,EquipmentAction::PrimaryUse,EquipmentHand::Primary);
 state.Begin(twin,EquipmentAction::PrimaryUse,EquipmentHand::OffHand);state.Update(1,true);
 if(!state.RetargetActive(twin,EquipmentAction::Release,EquipmentHand::OffHand) ||
    state.Channels()[0].definition.action!=EquipmentAction::PrimaryUse ||
    state.Channels()[1].definition.action!=EquipmentAction::Release)return 23;
 bool primaryEvent=false,offHandEvent=false;
 for(const auto& e:state.TakeEvents()) {
    primaryEvent|=e.ownerHand==EquipmentHand::Primary && e.hands==1;
    offHandEvent|=e.ownerHand==EquipmentHand::OffHand && e.hands==2;
 }
 if(!primaryEvent||!offHandEvent)return 24;
 state.Reset();
 shield.actions[0]={EquipmentAction::Block,"Block",0,0,0,.2f,false,true,true};
 if(!ValidateEquipmentDefinition(shield) ||
    !state.Begin(sword,EquipmentAction::PrimaryUse,EquipmentHand::Primary) ||
    !state.Begin(shield,EquipmentAction::Block,EquipmentHand::OffHand))return 25;
 state.Update(1,true);
 if(state.Channels()[0].phase!=EquipmentActionPhase::Active ||
    state.Channels()[1].phase!=EquipmentActionPhase::Active ||
    state.Cancel(shield.assetId,EquipmentHand::OffHand) ||
    state.CompleteActive(shield.assetId,EquipmentHand::OffHand))return 26;
 state.Update(20,true);
 if(state.ReservedHands()!=3 ||
    !state.Release(shield.assetId,EquipmentHand::OffHand))return 27;
 state.Update(0,true);
 if(state.Channels()[1].phase!=EquipmentActionPhase::Active)return 28;
 state.Update(.05f,true);
 if(state.Channels()[1].phase!=EquipmentActionPhase::Recovery ||
    state.Channels()[0].phase!=EquipmentActionPhase::Active)return 29;
 if(!state.CompleteActive(sword.assetId,EquipmentHand::Primary))return 30;
 state.Update(1,true);
 if(state.ReservedHands()!=0)return 31;
 state.Reset();
 state.Begin(sword,EquipmentAction::PrimaryUse,EquipmentHand::Primary);
 state.Begin(shield,EquipmentAction::Block,EquipmentHand::OffHand);
 state.Update(1,true,1);
 if(state.Channels()[0].phase!=EquipmentActionPhase::Prepare ||
    state.Channels()[1].phase!=EquipmentActionPhase::Active)return 32;
 if(state.Release(shield.assetId,static_cast<EquipmentHand>(99)))return 33;
 bad=shield;bad.actions[0].holdUntilRelease=true;
 if(ValidateEquipmentDefinition(bad))return 34;
 bad=shield;bad.actions[0].activeSeconds=.1f;
 if(ValidateEquipmentDefinition(bad))return 35;

 std::cout<<"Equipment actions: independent hands, two-hand exclusion, phased events, hold/release, cancellation, pause and reset PASS\n";
}
