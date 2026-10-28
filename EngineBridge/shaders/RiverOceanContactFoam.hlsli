// Opt-in river experiment: shore foam copied from pinned oceanSurfacePS.hlsl.
// Native WATER normal/reflection/refraction computations above remain unchanged.
// FFT wave-crest foam needs the ocean gradient texture and is deliberately absent.
#ifdef WATER
    [branch]
    if (camera.texture_depth_index >= 0 && camera.position.y > surface.P.y)
    {
        float riverLinearDepth = compute_lineardepth(input.pos.z);
        float solidLinearDepth = compute_lineardepth(texture_depth[uint2(input.pos.xy)]);
        float water_depth_diff = abs(solidLinearDepth - riverLinearDepth);
        // userdata.x is an experimental contact-width multiplier (float bits).
        // 1.0 reproduces the ocean shore distance falloff.
        float widthScale = clamp(asfloat(material.userdata.x), 0.05, 2.0);
        float foam_shore = saturate(exp(-water_depth_diff * 2 / widthScale));
        float foam_combined = foam_shore;
        float foam = smoothstep(0.5, 0.6, saturate(foam_combined + 0.1));
        [branch]
        if (foam_combined > 0.002)
        {
            float foam_simplex = 0;
            foam_simplex += smoothstep(0, 0.8, noise_simplex_2D(surface.P.xz * 1 + GetTime()));
            foam_simplex += smoothstep(0, 0.8, noise_simplex_2D(surface.P.xz * 2 + GetTime()));
            foam_simplex += smoothstep(0, 0.8, noise_simplex_2D(surface.P.zx * 4 - GetTime() * 2));
            float foam_voronoi = 0;
            foam_voronoi += smoothstep(0.5, 0.8, noise_voronoi(surface.P.xz * 1, GetTime()).x);
            foam_voronoi += smoothstep(0.5, 0.8, noise_voronoi(surface.P.xz * 2, GetTime()).x);
            foam_voronoi += smoothstep(0.5, 0.8, noise_voronoi(surface.P.xz * 4, GetTime()).x);
            foam += foam_voronoi * foam_simplex * foam_combined;
        }
        // River-scale adaptation: reject projected halos from surfaces above
        // the water, constrain the real intersection and break up the solid band.
        float3 solidPosition = reconstruct_position(ScreenCoord, texture_depth[uint2(input.pos.xy)]);
        float verticalGap = abs(solidPosition.y - surface.P.y);
        float trueContact = 1 - smoothstep(0.008, 0.09, verticalGap);
        float fineBreakup = smoothstep(-0.28, 0.45,
            noise_simplex_2D(surface.P.xz * 22 + float2(GetTime() * 0.8, -GetTime() * 1.4)));
        foam = saturate(foam * 2) * trueContact * (0.15 + 0.85 * fineBreakup);
        surface.albedo = lerp(surface.albedo, 0.6, foam);
        surface.refraction.a *= 1 - foam;

    }
#endif
