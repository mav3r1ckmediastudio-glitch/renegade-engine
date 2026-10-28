// G4 diagnostic only. Runs exclusively within the native WATER pixel shader.
#ifdef WATER
    if (material.userdata.y == 0x52474c34)
    {
        ByteAddressBuffer maskParams = bindless_buffers[descriptor_index(material.userdata.x)];
        float4 bounds = asfloat(maskParams.Load4(0));
        uint4 control = maskParams.Load4(16);
        float2 uv = (surface.P.xz - bounds.xy) / bounds.zw;
        Texture2D<float4> maskTexture = bindless_textures[descriptor_index(control.x)];
        int2 pixel = clamp(int2(floor(uv * float2(control.yz))), int2(0,0), int2(control.yz)-1);
        float4 mask = maskTexture.Load(int3(pixel,0));
        uint category = any(uv<0) || any(uv>1) ? 0 : (uint)round(mask.r);
        float3 diagnostic = float3(0.02,0.02,0.02);
        if (category == 1) diagnostic=float3(0.07,0.25,0.7); // Open WATER.
        if (category == 2) diagnostic=float3(0.95,0.7,0.02); // Terrain bank/shallow water.
        if (category == 3) diagnostic=float3(1,0.025,0.015); // Actual rock-water intersection.
        if (category == 4) diagnostic=float3(0.02,0.85,0.85); // Fully submerged mesh footprint.
        if (category == 5) diagnostic=float3(0.65,0.15,0.8); // Above-water footprint; excluded source.
        if (control.w == 1 && category != 3) diagnostic=float3(0.03,0.08,0.12);
        if (control.w == 2 && category != 2) diagnostic=float3(0.03,0.08,0.12);
        color.rgb=diagnostic;
    }
#endif
