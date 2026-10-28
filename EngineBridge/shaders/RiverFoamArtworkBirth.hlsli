// Original Renegade artwork; no third-party texture pixels or shader graph.
// The material tracer is born only where accepted foam sources are active.
Texture2D<float4> foamArtwork : register(t5);
float ArtworkAt(float2 world)
{
    uint width,height;foamArtwork.GetDimensions(width,height);
    float texelWorld=max(strands.bounds.z/strands.resolution.x,strands.bounds.w/strands.resolution.y);
    float lod=max(0,log2(max(.0001,texelWorld*width/4)));
    return foamArtwork.SampleLevel(sampler_linear_mirror,world/4,lod).r;
}
float Birth(float2 world,float time,float2 flow)
{
    float period=1.5,p0=frac(time/period),p1=frac(time/period+.5);
    float blend=.5-.5*cos(p0*6.2831853);
    return lerp(ArtworkAt(world-flow*p1*period),ArtworkAt(world-flow*p0*period),blend);
}
