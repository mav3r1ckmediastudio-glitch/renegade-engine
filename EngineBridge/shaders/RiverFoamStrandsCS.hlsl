// G8E optional source-born material tracer. Does not generate foam mass.
// Two-pass bounded MacCormack advection reduces first-order smearing.
// Density, discharge and obstacle classification remain the accepted simulation.
#include "globals.hlsli"
struct StrandPush {uint4 resolution;float4 bounds;float4 settings;};
PUSHCONSTANT(strands,StrandPush);
Texture2D<float> previousTracer : register(t0);
Texture2D<float> forwardTracer : register(t1);
Texture2D<float4> sources : register(t2);
Texture2D<float4> velocityField : register(t3);
Texture2D<float4> bulkFoam : register(t4);
RWTexture2D<float> result : register(u0);
float2 Velocity(float2 uv)
{
    if(any(uv<0)||any(uv>1))return 0;
    float4 q=velocityField.SampleLevel(sampler_linear_clamp,uv,0);
    return q.a>.99?q.xy*q.z*strands.settings.w:0;
}
float2 Trace(float2 uv,float dt)
{
    float2 v=Velocity(uv);
    float2 mid=uv-v*(dt*.5)/strands.bounds.zw;
    float4 wet=velocityField.SampleLevel(sampler_linear_clamp,mid,0);
    if(wet.a<.99||any(mid<0)||any(mid>1))return uv;
    float2 departure=uv-Velocity(mid)*dt/strands.bounds.zw;
    wet=velocityField.SampleLevel(sampler_linear_clamp,departure,0);
    return wet.a>.99&&all(departure>=0)&&all(departure<=1)?departure:uv;
}
float Hash(float3 p){return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453);}
float Birth(float2 world,float time)
{
    // Smooth source-only tracer labels, not a texture scrolling over the river.
    float3 p=float3(world*1.9,time*.16),i=floor(p),f=frac(p);f=f*f*(3-2*f);
    float a=lerp(lerp(Hash(i),Hash(i+float3(1,0,0)),f.x),
        lerp(Hash(i+float3(0,1,0)),Hash(i+float3(1,1,0)),f.x),f.y);
    float b=lerp(lerp(Hash(i+float3(0,0,1)),Hash(i+float3(1,0,1)),f.x),
        lerp(Hash(i+float3(0,1,1)),Hash(i+1),f.x),f.y);
    float label=lerp(a,b,f.z);
    return pow(saturate(1-abs(label-.5)/.085),.7);
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID)
{
    if(any(id.xy>=strands.resolution.xy))return;
    float2 uv=(float2(id.xy)+.5)/float2(strands.resolution.xy);
    float4 wet=velocityField.SampleLevel(sampler_linear_clamp,uv,0);
    float4 bulk=bulkFoam.SampleLevel(sampler_linear_clamp,uv,0);
    if(wet.a<.99||bulk.x<1e-7){result[id.xy]=0;return;}
    float dt=strands.settings.x;
    float2 departure=Trace(uv,dt);
    if(strands.resolution.z==0){
        result[id.xy]=previousTracer.SampleLevel(sampler_linear_clamp,departure,0);return;
    }
    float forward=forwardTracer.Load(int3(id.xy,0));
    float reverse=forwardTracer.SampleLevel(sampler_linear_clamp,Trace(uv,-dt),0);
    float corrected=forward+.5*(previousTracer.Load(int3(id.xy,0))-reverse);
    int2 base=int2(floor(departure*float2(strands.resolution.xy)-.5));
    float lo=1,hi=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){
        int2 cell=clamp(base+int2(x,y),int2(0,0),int2(strands.resolution.xy)-1);
        float q=previousTracer.Load(int3(cell,0));lo=min(lo,q);hi=max(hi,q);
    }
    corrected=clamp(corrected,lo,hi);
    float4 rate=sources.SampleLevel(sampler_linear_clamp,uv,0);
    float emission=0;
    [unroll]for(uint c=0;c<4;++c)if(strands.resolution.w&(1u<<c))emission+=rate[c];
    float2 world=strands.bounds.xy+uv*strands.bounds.zw;
    float weight=1-exp(-max(0,emission)*dt*8*strands.settings.w);
    corrected=lerp(corrected,Birth(world,strands.settings.y),weight);
    result[id.xy]=saturate(corrected);
}
