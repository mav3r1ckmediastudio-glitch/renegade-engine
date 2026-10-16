// Renegade Night Sky experimental contribution — Wicked atmosphere stays intact.
// Controls: gradient, aurora, auroraSpeed. Zero intensity = zero contribution.
#ifndef RENEGADE_NIGHT_SKY_HLSLI
#define RENEGADE_NIGHT_SKY_HLSLI
float RenegadeNightWeight(float3 sunDirection)
{
    // Fade from roughly 3 degrees below horizon to full night at 12 degrees.
    float depth = -normalize(sunDirection).y;
    float t = saturate((depth - 0.052336f) / (0.207912f - 0.052336f));
    return t * t * (3.0f - 2.0f * t);
}
float3 RenegadeNightGradient(float3 ray, float3 sunDirection, float strength)
{
    float weight = RenegadeNightWeight(sunDirection) * saturate(strength);
    [branch] if (weight <= 0.00001f) return 0;
    float up = normalize(ray).y;
    float z = smoothstep(0.15f, 0.95f, saturate(up));
    float horizon = pow(1.0f - smoothstep(0.0f, 0.55f, saturate(up)), 3.0f);
    // HDR linear colours; almost-black zenith, indigo mid and muted violet horizon.
    float3 color = lerp(float3(0.0065f, 0.0090f, 0.0200f),
                        float3(0.0018f, 0.0030f, 0.0085f), z);
    color += float3(0.0130f, 0.0120f, 0.0220f) * horizon;
    return color * weight * smoothstep(-0.035f, 0.03f, up);
}
float RenegadeAuroraHash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}
float RenegadeAuroraNoise(float2 p)
{
    float2 i = floor(p), f = frac(p), u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(RenegadeAuroraHash(i), RenegadeAuroraHash(i + float2(1, 0)), u.x),
                lerp(RenegadeAuroraHash(i + float2(0, 1)), RenegadeAuroraHash(i + float2(1, 1)), u.x), u.y);
}
float3 RenegadeAurora(float3 ray, float3 sunDirection, float intensity, float speed, float seconds)
{
    float weight = RenegadeNightWeight(sunDirection) * saturate(intensity);
    [branch] if (weight <= 0.0001f) return 0;
    float3 V = normalize(ray);
    float angle = atan2(V.x, V.z);
    float elev = saturate(V.y);
    float t = seconds * clamp(speed, 0.0f, 2.0f);
    // Limited azimuth span, soft upper and lower edges; no fullscreen overlay.
    float extent = 1.0f - smoothstep(1.05f, 2.15f, abs(angle));
    float height = smoothstep(0.05f, 0.20f, elev) *
                   (1.0f - smoothstep(0.77f, 0.96f, elev));
    float bend = 0.24f * sin(elev * 14.0f + t * 0.22f) +
                 0.08f * sin(elev * 27.0f - t * 0.13f);
    float ridge = pow(saturate(0.5f + 0.5f * sin(angle * 6.0f + bend +
                  0.40f * sin(angle * 3.0f + t * 0.11f))), 6.0f);
    float shimmer = 0.75f + 0.25f * RenegadeAuroraNoise(float2(angle * 5.0f, elev * 14.0f + t * 0.07f));
    float purple = smoothstep(0.55f, 0.85f, elev) * 0.55f;
    return lerp(float3(0.017f, 0.100f, 0.045f),
                float3(0.066f, 0.021f, 0.095f), purple) *
           extent * height * ridge * shimmer * weight;
}
#endif
