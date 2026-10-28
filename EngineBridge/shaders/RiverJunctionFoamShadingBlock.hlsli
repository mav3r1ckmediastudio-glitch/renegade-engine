// G7C: transported foam is an opaque, rough, lit bubble fraction of native WATER.
// This block runs AFTER native ApplyLighting; uncovered WATER remains exact.
// Density is generated/advected separately; procedural detail can never emit foam.
#ifdef WATER
    if(material.userdata.y==0x52464c35)
    {
        ByteAddressBuffer foamParams=bindless_buffers[descriptor_index(material.userdata.x)];
        float4 bounds=asfloat(foamParams.Load4(0));
        uint4 controls=foamParams.Load4(48);
        float4 options=asfloat(foamParams.Load4(64));
        uint stateIndex=(material.userdata.z&1)==0?controls.x:controls.y;
        Texture2D<float4> foamTexture=bindless_textures[descriptor_index(stateIndex)];
        Texture2D<float4> sourceTexture=bindless_textures[descriptor_index(controls.z)];
        float2 uv=(surface.P.xz-bounds.xy)/bounds.zw;
        float4 state=foamTexture.SampleLevel(sampler_linear_clamp,uv,0);
        float density=state.r*options.x;
        if(any(uv<0)||any(uv>1))density=0;
        if(options.z>0){
            color.rgb=lerp(float3(0,.02,.05),float3(1,.2,.02),saturate(density*3));
        }
        else if(density>0.0005)
        {
            float age=state.g/max(state.r,0.00001);
            float2 coordinate=state.ba/max(state.r,.00001);
            float scale=lerp(55,32,saturate(age/5));
            uint4 flowControl=foamParams.Load4(16);
            Texture2D<float4> baseFlow=bindless_textures[descriptor_index(flowControl.x)];
            float4 base=baseFlow.SampleLevel(sampler_linear_clamp,uv,0);
            float2 velocity=base.rg*base.b;
            float time=asfloat(material.userdata.w),period=.9;
            float phase=frac(time/period),phase2=frac(time/period+.5),blend=1-abs(2*phase-1);
            // Macro patches follow advected coordinates; fine bubbles retain round scale.
            float cell0=noise_voronoi((surface.P.xz-velocity*phase*period)*scale,age*.18).x;
            float cell1=noise_voronoi((surface.P.xz-velocity*phase2*period)*scale,age*.18).x;
            float bubbles=lerp(smoothstep(.19,.39,cell1),smoothstep(.19,.39,cell0),blend);
            float footprint=max(length(ddx_coarse(surface.P.xz)),length(ddy_coarse(surface.P.xz)))*scale;
            bubbles=lerp(bubbles,.72,smoothstep(.45,1.3,footprint));
            // Material coordinates carry large structures; world-metric phase
            // detail prevents compression at converging flow from making white ribbons.
            float2 detail0=surface.P.xz-velocity*phase*period;
            float2 detail1=surface.P.xz-velocity*phase2*period;
            float clump0=noise_simplex_2D(detail0*7.3)*.29+
                noise_simplex_2D(detail0*19.7)*.16+noise_simplex_2D(detail0*43.1)*.08;
            float clump1=noise_simplex_2D(detail1*7.3)*.29+
                noise_simplex_2D(detail1*19.7)*.16+noise_simplex_2D(detail1*43.1)*.08;
            float clusters=.5+lerp(clump1,clump0,blend)+noise_simplex_2D(coordinate*3.1)*.12;
            float mass=1-exp(-density*2);
            float coverage=smoothstep(.78-.40*mass+options.y-.06,.78-.40*mass+options.y+.06,clusters);
            float foam=saturate(coverage*(.36+.64*bubbles)*pow(mass,.8)*(.35+.65*smoothstep(.25,.7,clusters)));
            // Native rough dielectric material lighting, driven by transported density.
            // No screen-depth shore term or additive depth halo.
            // WATER Fresnel/refraction cannot shade opaque air bubbles correctly.
            // Evaluate a rough dielectric lobe with native tiled lights and shadows,
            // then mix by transported subpixel coverage on the SAME water surface.
            Surface bubbleSurface=surface;
            bubbleSurface.albedo=float3(.62,.645,.66);
            bubbleSurface.f0=.02;
            bubbleSurface.roughness=.8;
            bubbleSurface.N=normalize(lerp(surface.N,float3(0,1,0),.85));
            bubbleSurface.sss=0;
            bubbleSurface.transmission=0;
            bubbleSurface.refraction=0;
            bubbleSurface.extinction=1;
            bubbleSurface.emissiveColor=0;
            bubbleSurface.update();
            Lighting bubbleLighting;
            bubbleLighting.create(0,0,GetAmbient(bubbleSurface.N),0);
            TiledLighting(bubbleSurface,bubbleLighting,flat_tile_index,camera);
            half4 bubbleColor=half4(0,0,0,1);
            ApplyLighting(bubbleSurface,bubbleLighting,bubbleColor);
            color.rgb=lerp(color.rgb,bubbleColor.rgb,foam);
        }

    }
#endif
