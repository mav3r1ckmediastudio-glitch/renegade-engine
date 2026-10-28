// G7C proof-only conservative finite-volume foam transport.
#include "globals.hlsli"
struct JunctionPush {uint4 resolution;float4 bounds;float4 settings;};
PUSHCONSTANT(sim,JunctionPush);
Texture2D<float4> previousState : register(t0);
Texture2D<float4> faceFlux : register(t1);
Texture2D<float4> sourceMap : register(t2);
Texture2D<float4> velocityMap : register(t3);
RWTexture2D<float4> nextState : register(u0);
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=sim.resolution.xy))return;
    int2 cell=int2(id.xy);float4 velocity=velocityMap.Load(int3(cell,0));
    if(velocity.a<.5){nextState[cell]=0;return;}
    float4 initial=previousState.Load(int3(cell,0)),value=initial;
    float dt=sim.settings.x,time=sim.settings.y;
    if(dt==0){nextState[cell]=initial;return;}
    float4 flux=faceFlux.Load(int3(cell,0))*sim.settings.w;
    const int2 offsets[4]={int2(1,0),int2(0,1),int2(-1,0),int2(0,-1)};
    [unroll]for(int side=0;side<4;++side) {
        int2 n=cell+offsets[side];float4 other=0;
        if(all(n>=0)&&all(n<int2(sim.resolution.xy))&&velocityMap.Load(int3(n,0)).a>.5)
            other=previousState.Load(int3(n,0));
        value-=dt*flux[side]*(float(sim.resolution.x)*sim.resolution.y/(sim.bounds.z*sim.bounds.w))*(flux[side]>=0?initial:other);
    }
    float2 world=sim.bounds.xy+(float2(cell)+.5)/float2(sim.resolution.xy)*sim.bounds.zw;
    float pulse=.35+.65*(.5+.5*sin(world.x*8.3+world.y*5.7-time*2));
    float4 rates=sourceMap.Load(int3(cell,0));float emission=0;
    [unroll]for(uint c=0;c<4;++c)if(sim.resolution.z&(1u<<c))emission+=rates[c];
    float added=emission*pulse*sim.settings.w*dt,decay=exp(-dt/sim.settings.z);
    value.y=(value.y+value.x*dt)*decay;value.x=value.x*decay+added;
    value.zw=value.zw*decay+(world-velocity.xy*velocity.z*time)*added;
    if(value.x<1e-7||value.y/max(value.x,1e-7)>18)value=0;
    nextState[cell]=value;
}
