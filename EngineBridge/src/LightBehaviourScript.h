#pragma once
namespace renegade::bridge {
inline constexpr const char LightBehaviourLua[]=R"LUA(
-- Renegade stock light behaviour v1. Project copy is authoritative.
if renegade and renegade.metadata then
 renegade.metadata({
  schema_version=1, name="Light Behaviour", category="Lighting", role="SCRIPT",
  properties={
   {name="mode",label="Behaviour",type="enum",default="Flame",
    options={{value="Flame",label="Flame"},{value="Electrical Fault",label="Electrical Fault"},
     {value="Rotating",label="Rotating"},{value="Pulsing",label="Pulsing"},{value="Strobe",label="Strobe"}}},
   {name="strength",label="Strength",type="float",default=0.35,min=0,max=1},
   {name="speed",label="Speed",type="float",default=1.2,min=0.05,max=20},
   {name="movement",label="Movement (m)",type="float",default=0.03,min=0,max=0.5},
   {name="randomise",label="Randomise timing",type="boolean",default=true}
  }})
end
local base, clock, phase
local function restore(ctx)
 if base then renegade.light.set_state(ctx.entity,base) end
end
return {
 on_start=function(ctx)
  base=assert(renegade.light.get_state(ctx.entity))
  clock=0
  phase=ctx.properties.randomise and base.seed*100 or 0
 end,
 on_update=function(ctx,dt)
  if not base then return end
  clock=clock+dt
  local p=ctx.properties
  local t=clock*p.speed+phase
  local a=p.strength
  local factor=1
  local state={}
  for k,v in pairs(base) do state[k]=v end
  if p.mode=="Flame" then
   -- Smooth mixed frequencies give irregular flicker without frame-rate random noise.
   local n=0.52*math.sin(t*8.3)+0.3*math.sin(t*17.7+1.4)+0.18*math.sin(t*31.1+3)
   factor=1-a*(0.5+0.5*n)
   state.g=base.g*(1-a*0.12*(0.5+0.5*n))
   state.b=base.b*(1-a*0.2*(0.5+0.5*n))
   state.x=base.x+p.movement*math.sin(t*2.3)
   state.y=base.y+p.movement*math.sin(t*3.7+1)
   state.z=base.z+p.movement*math.sin(t*2.9+2)
  elseif p.mode=="Electrical Fault" then
   local n=math.sin(t*8.7)+math.sin(t*23.3+2)+0.5*math.sin(t*41)
   factor=n>0.75 and (1-a) or 1-a*0.12*(0.5+0.5*math.sin(t*67))
  elseif p.mode=="Rotating" then
   local s,c=math.sin(clock*p.speed*0.5),math.cos(clock*p.speed*0.5)
   state.qx=base.qx*c+base.qz*s
   state.qy=base.qy*c+base.qw*s
   state.qz=base.qz*c-base.qx*s
   state.qw=base.qw*c-base.qy*s
  elseif p.mode=="Pulsing" then
   factor=1-a*(0.5+0.5*math.sin(t*math.pi*2))
  elseif p.mode=="Strobe" then
   factor=(t%1)<0.2 and 1 or (1-a)
  end
  state.intensity=base.intensity*factor
  assert(renegade.light.set_state(ctx.entity,state))
 end,
 on_stop=restore
}
)LUA";
}
