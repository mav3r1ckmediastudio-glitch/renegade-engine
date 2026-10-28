// Renegade-owned G3 proof extension. Replaces ONLY the two native normal samples.
// Transient raw buffer layout (48 bytes): world bounds; texture/time/sign/mode;
// tile metres, phase period, running time offset, enabled. Never serialize it.
        bumpColor0 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap - material.texMulAdd.ww).rg - 1;
        bumpColor1 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap + material.texMulAdd.zw).rg - 1;
        if (material.userdata.y == 0x52464c33)
        {
            ByteAddressBuffer flowParams = bindless_buffers[descriptor_index(material.userdata.x)];
            float4 bounds = asfloat(flowParams.Load4(0));
            uint4 controlBits = flowParams.Load4(16);
            float4 flowSettings = asfloat(flowParams.Load4(32));
            Texture2D<float4> flowTexture = bindless_textures[descriptor_index(controlBits.x)];
            float2 flowUV = (surface.P.xz - bounds.xy) / bounds.zw;
            float4 flow = flowTexture.SampleLevel(sampler_linear_clamp, flowUV, 0);
            float mask = smoothstep(0.5, 0.99, flow.a) * flowSettings.w;
            if (any(flowUV < 0) || any(flowUV > 1)) mask = 0;
            float2 direction = flow.rg / max(flow.a, 0.0001);
            float speed = flow.b / max(flow.a, 0.0001);
            float2 worldVelocity = length(direction) > 0.001 ? normalize(direction) * speed * asfloat(controlBits.z) : 0;
            riverFlowDebug = float4(direction * asfloat(controlBits.z), speed, mask);
            riverFlowDebugMode = controlBits.w;
            if (mask > 0 && material.textures[NORMALMAP].GetUVSet() == 0)
            {
                float tileMetres = flowSettings.x;
                float period = flowSettings.y;
                float clock = asfloat(controlBits.y);
                clock = clock >= 0 ? clock : GetTime() + flowSettings.z;
                // Native ribbon U runs LEFT across banks, V runs forward.
                // Preserve Wicked's normal-map channels and its original TBN.
                float2 uvVelocity = float2(-dot(worldVelocity, normalize(TBN[1].xz)),
                    dot(worldVelocity, normalize(TBN[0].xz))) / tileMetres;
                float2 baseUV = UV_normalMap - material.texMulAdd.zw;
                float2 dx = ddx_coarse(baseUV), dy = ddy_coarse(baseUV);
                float p0 = frac(clock / period), p1 = frac(clock / period + 0.5);
                float w0 = 1 - abs(2 * p0 - 1);
                float2 a0 = 2 * texture_normalmap.SampleGrad(sampler_objectshader,
                    baseUV - uvVelocity * (p0 * period), dx, dy).rg - 1;
                float2 a1 = 2 * texture_normalmap.SampleGrad(sampler_objectshader,
                    baseUV - uvVelocity * (p1 * period), dx, dy).rg - 1;
                float q0 = frac(clock / period + 0.23), q1 = frac(clock / period + 0.73);
                float v0 = 1 - abs(2 * q0 - 1);
                float2 detailUV = baseUV + float2(0.271, 0.619);
                float2 b0 = 2 * texture_normalmap.SampleGrad(sampler_objectshader,
                    detailUV - uvVelocity * (q0 * period), dx, dy).rg - 1;
                float2 b1 = 2 * texture_normalmap.SampleGrad(sampler_objectshader,
                    detailUV - uvVelocity * (q1 * period), dx, dy).rg - 1;
                bumpColor0 = lerp(bumpColor0, a0 * w0 + a1 * (1 - w0), mask);
                bumpColor1 = lerp(bumpColor1, b0 * v0 + b1 * (1 - v0), mask);
            }
        }
