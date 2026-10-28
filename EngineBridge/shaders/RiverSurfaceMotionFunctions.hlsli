// G8D optional VS proof. Same native WATER surface, bounded displacement.
// A shared baked phase/amplitude field spans the actual native junction footprint.
float RiverSurfaceHeight(Texture2D<float4> field,float2 uv,float time)
{
    float4 q=field.SampleLevel(sampler_linear_clamp,uv,0);
    // Wavelengths 1.9 / 1.05 / .62 m; downstream celerities 1.9 / 1.575 / 1.24 m/s.
    // Harmonic temporal frequencies avoid a discontinuity at the 256-second clock wrap.
    float phase=q.x;
    float wave=.60*sin(6.2831853*(phase/1.9-time))
        +.27*sin(6.2831853*(phase/1.05-time*1.5)+.8)
        +.13*sin(6.2831853*(phase/.62-time*2)+1.7);
    return q.y*wave;
}
