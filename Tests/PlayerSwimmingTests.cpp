#include "renegade/bridge/PlayerSwimmingService.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
void Require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
 PlayerInputFrame input;input.moveForward=1;
 auto down=EvaluatePlayerSwimmingVelocity(input,0,XM_PIDIV4,-1,0);
 Require(down.y<-2 && down.z>2,"look down + W dives");
 auto up=EvaluatePlayerSwimmingVelocity(input,0,-XM_PIDIV4,-1,0);
 Require(up.y>2 && up.z>2,"look up + W ascends");
 auto yaw=EvaluatePlayerSwimmingVelocity(input,XM_PIDIV2,0,-1,0);
 Require(yaw.x>2.9f && std::abs(yaw.z)<.001f,"yaw follows mouse");
 input.moveRight=1;auto diagonal=EvaluatePlayerSwimmingVelocity(input,0,XM_PIDIV4,-1,0);
 Require(std::abs(std::sqrt(diagonal.x*diagonal.x+diagonal.y*diagonal.y+diagonal.z*diagonal.z)-4.5f*.75f)<.001f,"diagonal swim speed is 75 percent of walk");
 input.moveRight=0;input.sprintDown=true;
 auto sprint=EvaluatePlayerSwimmingVelocity(input,0,0,-1,0);
 Require(std::abs(sprint.z-7.5f*.75f)<.001f,"sprint swim speed is 75 percent of land sprint");
 input.sprintDown=false;
 input.moveRight=0;input.moveForward=.5f;
 auto analog=EvaluatePlayerSwimmingVelocity(input,0,XM_PIDIV4,-1,0);
 Require(std::abs(analog.y-down.y*.5f)<.001f,"analog dive preserved");
 input={};auto idle=EvaluatePlayerSwimmingVelocity(input,0,0,-2,0);
 Require(idle.y>0 && idle.y<=.35f,"idle float gentle");
 input.moveForward=1;
 Require(EvaluatePlayerInput(input,0,XM_PIDIV4).direction.y==0,"land walking remains horizontal");
 wi::scene::Scene scene;RuntimePlayerState state;
 Require(!TryMovePlayerInRiver(scene,state,input,{}),"missing live body fails safely");
 std::cout<<"PASS: dive, ascend, yaw, 25% slower swim/sprint, diagonal, analog, idle float, land movement\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
