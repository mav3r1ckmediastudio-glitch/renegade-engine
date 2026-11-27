// Reconstruct straight RGBA from black-backed RGB video. The normal alpha
// compositor then preserves the screen under empty pixels and partial coverage.
Texture2D<float4> source : register(t0);
RWTexture2D<float4> result : register(u0);
cbuffer MatteSettings : register(b999) { uint softSmoke; };
[RootSignature("RootConstants(num32BitConstants=1,b999), DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0))")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width,height;
    result.GetDimensions(width,height);
    if (id.x>=width || id.y>=height) return;
    float3 rgb=source.Load(int3(id.xy,0)).rgb;
    float value=max(rgb.r,max(rgb.g,rgb.b));
    float alpha=smoothstep(0.025,0.115,value);
    float edge=1;
    if (softSmoke)
    {
        alpha=saturate((value-0.025)/0.65);
        float2 uv=(float2(id.xy)+0.5)/float2(width,height);
        float2 distanceToEdge=min(uv,1-uv);
        edge=smoothstep(0,0.055,distanceToEdge.x)*smoothstep(0,0.10,distanceToEdge.y);
    }
    result[id.xy]=float4(alpha>0 ? saturate(rgb/max(alpha,0.001)) : 0,alpha*edge);
}
