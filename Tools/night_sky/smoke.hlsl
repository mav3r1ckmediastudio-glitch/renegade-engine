// DXC compile check for the standalone Night Sky shader module.
// This does not exercise Wicked's full sky pass or visual output.
#include "renegadeNightSky.hlsli"
float4 main(float4 pos : SV_Position) : SV_Target
{
    float3 view = normalize(float3(pos.x / 1000.0f, 0.5f, pos.y / 1000.0f));
    float3 sun = normalize(float3(0.1f, -0.75f, 0.1f));
    float3 night = RenegadeNightGradient(view, sun, 1.0f, float4(0.0018,0.003,0.0085,0), float4(0.013,0.012,0.022,1));
    float3 aurora = RenegadeAurora(view, sun, 0.5f, 0.35f, 0.4f, 5.0f);
    return float4(night + aurora, 1.0f);
}
