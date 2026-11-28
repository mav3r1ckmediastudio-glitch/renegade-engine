// Generic masked fill. The bound fraction controls level; time moves only
// the surface and bubbles. Mask brightness and alpha both define coverage.
Texture2D<float4> source : register(t0);
RWTexture2D<float4> result : register(u0);
cbuffer LiquidSettings : register(b999)
{
    float left,right,top,bottom;
    float fraction,seconds,wavePixels,packedMotion;
};
[RootSignature("RootConstants(num32BitConstants=8,b999), DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0))")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width,height;result.GetDimensions(width,height);
    if(id.x>=width||id.y>=height)return;
    float2 uv=(float2(id.xy)+.5)/float2(width,height);
    float4 sample=source.Load(int3(id.xy,0));float mask=sample.r*sample.a;
    uint direction=(uint)packedMotion/64;uint bubbles=(uint)packedMotion%64;
    bool horizontal=direction<2;
    float axisLength=horizontal?(right-left)*width:(bottom-top)*height;
    float p=horizontal?(uv.x-left)/max(.001,right-left):(uv.y-top)/max(.001,bottom-top);
    bool reverse=direction==1||direction==2;if(reverse)p=1-p;
    float cross=horizontal?(uv.y-top)/max(.001,bottom-top):(uv.x-left)/max(.001,right-left);
    float wave=wavePixels/max(1,axisLength)*sin(cross*6.28318530718+seconds*1.57079632679);
    float level=saturate(fraction)+(fraction>=1?0:wave);
    if(fraction<=0||mask<=0||p>level||uv.x<left||uv.x>right||uv.y<top||uv.y>bottom)
    {result[id.xy]=0;return;}
    float depth=saturate((level-p)/max(.001,level));float value=lerp(.51,.32,depth);
    float meniscus=1-smoothstep(0,1.4/max(1,axisLength),abs(p-level));value=lerp(value,.88,meniscus*.65);
    for(uint i=0;i<bubbles;++i)
    {
        float q=frac(seconds*.25+i/max(1.0,float(bubbles)));
        float axis=level*q;if(reverse)axis=1-axis;
        float across=.5+.27*sin(i*2.3);
        float2 centre=horizontal?float2(lerp(left,right,axis),lerp(top,bottom,across)):
            float2(lerp(left,right,across),lerp(top,bottom,axis));
        float distance=length((uv-centre)*float2(width,height));float radius=1+(i%2)*.4;
        float ring=(1-smoothstep(.35,1,abs(distance-radius)))*pow(sin(3.14159265*q),2);
        value=lerp(value,.9,ring*.75);
    }
    result[id.xy]=float4(value,value,value,.92*mask);
}
