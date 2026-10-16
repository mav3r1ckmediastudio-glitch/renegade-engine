// Renegade Aurora Borealis — separate procedural animated curtain shader module.
#ifndef RENEGADE_AURORA_HLSLI
#define RENEGADE_AURORA_HLSLI
float RenegadeAuroraHash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f,311.7f))) * 43758.5453f);
}
float RenegadeAuroraNoise(float2 p)
{
    float2 i=floor(p), f=frac(p), u=f*f*(3.0f-2.0f*f);
    return lerp(lerp(RenegadeAuroraHash(i),RenegadeAuroraHash(i+float2(1,0)),u.x),
                lerp(RenegadeAuroraHash(i+float2(0,1)),RenegadeAuroraHash(i+float2(1,1)),u.x),u.y);
}
float3 RenegadeAurora(float3 direction, float3 sun,
                       float intensity, float speed, float coverage, float seconds)
{
    float weight = RenegadeNightWeight(sun) * saturate(intensity);
    [branch] if(weight <= 0.0001f) return 0;
    float3 V = normalize(direction);
    float angle = atan2(V.x,V.z);
    float elev = saturate(V.y);
    float t = seconds * clamp(speed,0.0f,2.0f);
    float span = lerp(0.4f,2.75f,saturate(coverage));
    float extent = 1.0f - smoothstep(span*0.55f,span,abs(angle));
    float height = smoothstep(0.05f,0.20f,elev) *
                   (1.0f-smoothstep(0.77f,0.96f,elev));
    float bend = 0.24f*sin(elev*14.0f+t*0.22f) +
                 0.08f*sin(elev*27.0f-t*0.13f);
    float ridge = pow(saturate(0.5f+0.5f*sin(angle*6.0f+bend +
                  0.40f*sin(angle*3.0f+t*0.11f))),6.0f);
    float shimmer=0.75f+0.25f*RenegadeAuroraNoise(float2(angle*5.0f,elev*14.0f+t*0.07f));
    float violet=smoothstep(0.55f,0.85f,elev)*0.55f;
    return lerp(float3(0.017f,0.100f,0.045f),
                float3(0.066f,0.021f,0.095f),violet) *
           extent*height*ridge*shimmer*weight;
}
#endif
