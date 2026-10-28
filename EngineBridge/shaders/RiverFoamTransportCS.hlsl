#include "globals.hlsli"
struct RiverFoamPush {uint4 resolution;float4 bounds;float4 settings;};
PUSHCONSTANT(sim, RiverFoamPush);
Texture2D<float4> previousState : register(t0);
Texture2D<float4> velocityMap : register(t1);
Texture2D<float4> sourceMap : register(t2);
Texture2D<float4> baseVelocityMap : register(t3);
RWTexture2D<float4> nextState : register(u0);
float2 velocityAt(float2 uv){float4 v=velocityMap.SampleLevel(sampler_linear_clamp,uv,0);return v.rg*v.b;}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID){
    if(any(id.xy>=sim.resolution.xy))return;
    if(all(id.xy==0)){nextState[id.xy]=float4(0,0,asfloat(sim.resolution.z),0);return;}
    float2 uv=(id.xy+.5)/float2(sim.resolution.xy);
    float4 velocity=velocityMap.Load(int3(id.xy,0)),source=sourceMap.Load(int3(id.xy,0));
    if(velocity.a<.5){nextState[id.xy]=0;return;}
    float dt=sim.settings.x,time=sim.settings.y;
    if(dt==0){nextState[id.xy]=previousState.Load(int3(id.xy,0));return;}
    float2 world=sim.bounds.xy+uv*sim.bounds.zw,v=velocity.rg*velocity.b;
    // Bounded divergence-free curl perturbation; never creates foam.
    float2 downstream=velocity.b>.001?velocity.rg:float2(1,0);
    float2 lateral=float2(-downstream.y,downstream.x);
    float along=dot(world,downstream)-time*.65;
    float index=floor(along/1.15),side=source.a;
    float2 vortices=0;
    [unroll]for(int i=-2;i<=2;++i){
        float n=index+i;
        float sign=frac(n*.5)<.25?1:-1;
        float jitter=frac(sin(n*12.37)*4358.19);
        float dx=along-(n+.5)*1.15,dy=side-sign*(.28+jitter*.12);
        float radius=.48+jitter*.13;
        float influence=exp(-(dx*dx+dy*dy)/(radius*radius));
        vortices+=(lateral*dx-downstream*dy)*sign*influence*9.0;
    }
    v+=vortices*(source.b*asfloat(sim.resolution.z));
    float2 back=uv-v*dt/sim.bounds.zw;
    // Prevent backtracing through a solid boundary, including thin obstacles.
    if(any(back<0)||any(back>1)){nextState[id.xy]=0;return;}
    float pass=1;
    [unroll]for(int i=1;i<=3;++i)
        pass*=step(.5,velocityMap.SampleLevel(sampler_point_clamp,lerp(uv,back,i/3.0),0).a);
    float4 state=pass>.5?previousState.SampleLevel(sampler_linear_clamp,back,0):0;
    float decay=exp(-dt/sim.settings.z);
    float pulse=smoothstep(-.25,.6,noise_simplex_2D(world*5.7+float2(-time*1.3,time*.41)));
    float emission=(source.r+source.g)*(.08+.92*pulse)*sim.settings.w;
    float added=emission*dt;
    float density=state.r*decay+added;
    float ageMass=(state.g+state.r*dt)*decay;
    float4 base=baseVelocityMap.SampleLevel(sampler_linear_clamp,uv,0);
    float2 freshCoordinate=world-base.rg*base.b*time;
    float2 coordinateMass=state.ba*decay+freshCoordinate*added;
    float limit=min(1,3.0/max(density,.000001));
    density*=limit;ageMass*=limit;coordinateMass*=limit;
    // Age and material coordinates travel with the generated foam mass.
    if(density<0.00001||ageMass/max(density,.00001)>9){density=0;ageMass=0;coordinateMass=0;}
    nextState[id.xy]=float4(density,ageMass,coordinateMass);
}
