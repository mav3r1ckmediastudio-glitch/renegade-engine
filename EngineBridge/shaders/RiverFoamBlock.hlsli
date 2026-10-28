// Experimental flat-water foam addition before native ApplyLighting.
// userdata is a transient descriptor/count/magic triplet, never authored scene data.
#ifdef WATER
    if (material.userdata.z == 0x52464f41 && material.userdata.y <= 16 &&
        camera.texture_depth_index >= 0 && camera.position.y > surface.P.y)
    {
        ByteAddressBuffer regions = bindless_buffers[descriptor_index(material.userdata.x)];
        float rawDepth = texture_depth.SampleLevel(sampler_point_clamp, ScreenCoord.xy, 0);
        float3 solid = reconstruct_position(ScreenCoord.xy, rawDepth);
        float gap = abs(surface.P.y - solid.y);
        float foamTotal = 0;
        for (uint regionIndex = 0; regionIndex < material.userdata.y; ++regionIndex)
        {
            uint address = regionIndex * 64;
            float4 centre = asfloat(regions.Load4(address));
            float4 flow = asfloat(regions.Load4(address + 16));
            float4 settings = asfloat(regions.Load4(address + 32));
            if(settings.y <= 0 || abs(surface.P.y - centre.y) > 0.05) continue;
            float2 offset = surface.P.xz - centre.xz;
            float along = dot(offset, flow.xy);
            float across = dot(offset, float2(flow.y, -flow.x));
            float downstream = along - centre.w * 0.45;
            if(downstream > flow.z || along < -centre.w * 1.6 ||
                abs(across) > centre.w * 1.8 + flow.w + max(downstream,0) * 0.2) continue;
            float contact = (1 - smoothstep(settings.z * 0.12, settings.z, gap)) *
                (1 - smoothstep(centre.w * 1.05, centre.w * 1.7, length(offset)));
            float travel = GetTime() * settings.x;
            // Advect in flow space; a second, slower field keeps the pattern from
            // appearing as a static white decal sliding along the river.
            float2 localUV = float2(across, along - travel);
            float broad = noise_simplex_2D(localUV * float2(3.2, 2.0)) * 0.5 + 0.5;
            float detail = noise_voronoi(localUV * 8, GetTime() * 0.2).x;
            float breakup = smoothstep(0.46, 0.86, broad + detail * 0.35);
            float wisps = smoothstep(0.48, 0.76,
                noise_simplex_2D(float2(across * 7.0, (along - travel * 1.3) * 4.0)) * 0.5 + 0.5);
            // A rock splits flow into two turbulent shoulders. The centre fills
            // unevenly after the obstacle rather than producing one bright strip.
            float bend = sin(downstream * 1.7) * 0.11 +
                sin(downstream * 3.9 + 1.1) * 0.045;
            float spread = centre.w * 0.62 + flow.w * 0.4 +
                max(downstream, 0) * 0.16;
            float shoulderWidth = max(flow.w * 0.65 + max(downstream,0) * 0.055, 0.08);
            float left = exp(-pow((across - bend - spread) / shoulderWidth, 2) * 1.6);
            float right = exp(-pow((across - bend + spread) / shoulderWidth, 2) * 1.6);
            float middle = exp(-pow((across - bend) /
                max(spread + shoulderWidth, 0.1), 2) * 1.8);
            float shoulderFade = 1 - smoothstep(flow.z * 0.30, flow.z, downstream);
            float wakeStart = smoothstep(-centre.w * 0.35,
                max(centre.w * 0.6, 0.05), downstream);
            float wake = wakeStart * shoulderFade *
                (max(left, right) * 0.85 + middle * 0.22 * wisps);
            // Local contact hugs the collision silhouette rather than lighting
            // the whole circular authored region.
            float brokenContact = contact * (0.55 + 0.45 * wisps);
            foamTotal = max(foamTotal, saturate(
                max(brokenContact, wake) * breakup * settings.y));
        }
        surface.albedo = lerp(surface.albedo, 0.75, foamTotal);
        surface.refraction.a *= 1 - foamTotal;
        surface.roughness = lerp(surface.roughness, 0.85, foamTotal);
    }
#endif
