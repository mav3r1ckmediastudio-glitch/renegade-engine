Texture2D<float4> sourceMap : register(t0);
RWTexture2D<float4> outputMap : register(u0);
cbuffer Settings : register(b999)
{
    float4 observer; // world X,Z; heading cos,sin
    float4 bounds; // west,east,south,north
    float4 options; // range, aspect, live, unused
};
[RootSignature("RootConstants(num32BitConstants=12,b999), DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0))")]
[numthreads(8,8,1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint w,h;outputMap.GetDimensions(w,h);if(thread.x>=w||thread.y>=h)return;
    float2 uv=(thread.xy+.5)/float2(w,h);
    if(options.z<.5){
        float right=(uv.x-.5)*2*options.x*options.y;
        float forward=(.5-uv.y)*2*options.x;
        float x=observer.x+observer.z*right+observer.w*forward;
        float z=observer.y-observer.w*right+observer.z*forward;
        uv=float2((x-bounds.x)/(bounds.y-bounds.x),(bounds.w-z)/(bounds.w-bounds.z));
    }
    if(any(uv<0)||any(uv>1)){outputMap[thread.xy]=0;return;}
    uint sw,sh;sourceMap.GetDimensions(sw,sh);
    // Bilinear sampling without a sampler dependency in the host command list.
    float2 texel=uv*float2(sw,sh)-.5;int2 p=int2(floor(texel));float2 blend=frac(texel);
    int2 limit=int2(sw-1,sh-1);
    float4 a=sourceMap.Load(int3(clamp(p,0,limit),0));
    float4 b=sourceMap.Load(int3(clamp(p+int2(1,0),0,limit),0));
    float4 c=sourceMap.Load(int3(clamp(p+int2(0,1),0,limit),0));
    float4 d=sourceMap.Load(int3(clamp(p+1,0,limit),0));
    outputMap[thread.xy]=lerp(lerp(a,b,blend.x),lerp(c,d,blend.x),blend.y);
}
