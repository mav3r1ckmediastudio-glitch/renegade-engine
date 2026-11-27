// A grayscale interior mask retains the supplied vial outline. Level comes
// from the bound Progress value; the independent clock only moves the surface.
Texture2D<float4> source : register(t0);
RWTexture2D<float4> result : register(u0);
cbuffer LiquidSettings : register(b999)
{
    float left,right,top,bottom;
    float fraction,seconds,wavePixels,bubbles;
};
[RootSignature("RootConstants(num32BitConstants=8,b999), DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0))")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width,height;result.GetDimensions(width,height);
    if(id.x>=width||id.y>=height)return;
    float2 uv=(float2(id.xy)+.5)/float2(width,height);
    float mask=source.Load(int3(id.xy,0)).r;
    float level=bottom-(bottom-top)*saturate(fraction);
    float phase=seconds*1.57079632679;
    float wave=wavePixels/height*sin((uv.x-left)/max(.001,right-left)*6.28318530718+phase);
    // Full/empty are exact endpoints, including a static reduced-motion state.
    if(fraction<=0||mask<=0||uv.y<level+(fraction>=1?0:wave))
    {result[id.xy]=0;return;}
    float depth=saturate((uv.y-level)/max(.001,bottom-level));
    float value=lerp(.51,.32,depth);
    float alpha=.92*mask;
    float meniscus=1-smoothstep(0,1.4/height,abs(uv.y-level-wave));
    value=lerp(value,.88,meniscus*.65);
    for(uint i=0;i<(uint)bubbles;++i)
    {
        float q=frac(seconds*.25+i/max(1,bubbles));
        float2 centre=float2((left+right)*.5+(right-left)*.27*sin(i*2.3),bottom-(bottom-level)*q);
        float radius=1+(i%2)*.4;
        float distance=length((uv-centre)*float2(width,height));
        float ring=(1-smoothstep(.35,1,abs(distance-radius)))*pow(sin(3.14159265*q),2);
        value=lerp(value,.9,ring*.75);
    }
    result[id.xy]=float4(value,value,value,alpha);
}
