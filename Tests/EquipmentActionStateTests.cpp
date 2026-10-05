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
 std::cout<<"Equipment actions: independent hands, two-hand exclusion, phased events, hold/release, cancellation, pause and reset PASS\n";
}
