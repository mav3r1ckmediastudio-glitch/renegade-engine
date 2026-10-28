#include "globals.hlsli"
#include "ShaderInterop_Postprocess.h"
PUSHCONSTANT(postprocess, PostProcess);
Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);
float4 load4(ByteAddressBuffer data,uint offset){return asfloat(data.Load4(offset));}
bool hitBox(float3 origin,float3 direction,float3 low,float3 high,float limit)
{
    float enter=0,leave=limit;
    [unroll] for(uint axis=0;axis<3;++axis)
    {
        if(abs(direction[axis])<1e-7){if(origin[axis]<low[axis]||origin[axis]>high[axis])return false;}
        else{float a=(low[axis]-origin[axis])/direction[axis],b=(high[axis]-origin[axis])/direction[axis];enter=max(enter,min(a,b));leave=min(leave,max(a,b));}
    }
    return leave>=enter;
}
bool hitPrism(ByteAddressBuffer data,uint offset,float3 origin,float3 direction,float limit,out float enter,out float leave)
{
    enter=0;leave=limit;
    [unroll] for(uint i=0;i<5;++i)
    {
        float4 plane=load4(data,offset+i*16);float distance=dot(plane.xyz,origin)+plane.w,denominator=dot(plane.xyz,direction);
        if(abs(denominator)<1e-7){if(distance>1e-5)return false;}
        else{float t=-distance/denominator;if(denominator<0)enter=max(enter,t);else leave=min(leave,t);}
        if(enter>leave)return false;
    }
    return leave>enter;
}
[numthreads(POSTPROCESS_BLOCKSIZE,POSTPROCESS_BLOCKSIZE,1)]
void main(uint3 tid:SV_DispatchThreadID)
{
    if(any(tid.xy>=postprocess.resolution))return;
    float2 uv=(tid.xy+.5)*postprocess.resolution_rcp;
    float4 original=input.Load(int3(tid.xy,0));output[tid.xy]=original;
    ByteAddressBuffer data=bindless_buffers[(uint)postprocess.params0.x];uint nodeCount=(uint)postprocess.params0.y;
    float3 origin=reconstruct_position(uv,1),direction=normalize(reconstruct_position(uv,.0001)-origin);
    // Native ocean already shaded submerged ocean pixels earlier in the chain.
    if(postprocess.params1.x>.5)
    {
        ShaderOcean ocean=GetWeather().ocean;float3 surface=float3(origin.x,ocean.water_height,origin.z);
        if(ocean.texture_displacementmap>=0)
        {
            Texture2D displacement=bindless_textures[descriptor_index(ocean.texture_displacementmap)];
            surface+=displacement.SampleLevel(sampler_linear_wrap,surface.xz*ocean.patch_size_rcp,0).xzy;
        }
        if(origin.y<=surface.y+.025)return;
    }
    float depth=texture_depth.SampleLevel(sampler_point_clamp,uv,0),limit=postprocess.params0.z;
    if(depth>0)limit=min(limit,length(reconstruct_position(uv,depth)-origin));
    if(limit<=.0001)return;
    float2 intervals[64];uint media[64];uint count=0,node=0;
    [loop] while(node<nodeCount)
    {
        float4 low=load4(data,node*32),high=load4(data,node*32+16);
        if(!hitBox(origin,direction,low.xyz-.0001,high.xyz+.0001,limit)){node=(uint)high.w;continue;}
        if(low.w>0)
        {
            uint offset=nodeCount*32+((uint)low.w-1)*112;float enter,leave;
            if(hitPrism(data,offset,origin,direction,limit,enter,leave))
            {
                // Merge adjacent native triangles immediately, independent of BVH order.
                // A long continuous river uses one interval rather than hundreds.
                uint at=0;
                [loop] while(at<count){if(intervals[at].y+.001>=enter)break;++at;}
                uint end=at;
                [loop] while(end<count)
                {
                    if(intervals[end].x>leave+.001)break;
                    if(intervals[end].x<=enter){enter=intervals[end].x;offset=media[end];}
                    leave=max(leave,intervals[end].y);++end;
                }
                if(end==at)
                {
                    if(count==64)return;
                    [loop] for(uint j=count;j>at;--j){intervals[j]=intervals[j-1];media[j]=media[j-1];}
                    ++count;
                }
                else
                {
                    uint removed=end-at-1;
                    [loop] for(uint j=end;j<count;++j){intervals[j-removed]=intervals[j];media[j-removed]=media[j];}
                    count-=removed;
                }
                intervals[at]=float2(enter,leave);media[at]=offset;
            }
        }
        ++node;
    }
    if(count==0||intervals[0].x>.0001)return;
    float distanceInWater=intervals[0].y;
    [loop] for(uint i=1;i<count;++i){if(intervals[i].x>distanceInWater+.001)break;distanceInWater=max(distanceInWater,intervals[i].y);}
    float4 tint=load4(data,media[0]+80),top=load4(data,media[0]);float3 extinction=load4(data,media[0]+96).rgb;
    float immersion=-(dot(top.xyz,origin)+top.w)/max(top.y,.15);
    float transition=smoothstep(0,.025,immersion);
    // Match the native ocean's underwater distance scale: retain a readable
    // riverbed at short ranges rather than turning clear water into dense fog.
    float opticalDistance=distanceInWater*.1;
    float3 transmission=exp(-extinction*opticalDistance);
    float scatter=1-exp(-tint.a*opticalDistance);
    float3 color=original.rgb*transmission+tint.rgb*scatter;
    output[tid.xy]=float4(lerp(original.rgb,color,transition),original.a);
}
