# The terrain tile baker gets its chunk origin from the native CPU residency pass.
# Keep legacy materials on Wicked's original UV and mip path.
_ns_replace("wiTerrain.cpp" "void Terrain::UpdateVirtualTexturesCPU()"
[=[struct GroundChunkVirtualTexture final : VirtualTexture
    {
        XMFLOAT3 groundFrame = {}; // left edge X, negative near edge Z, span in metres
    };
    void Terrain::UpdateVirtualTexturesCPU()]=] "struct GroundChunkVirtualTexture final")
_ns_replace("wiTerrain.cpp" "wi::allocator::make_shared<VirtualTexture>()"
    "wi::allocator::make_shared<GroundChunkVirtualTexture>()" "make_shared<GroundChunkVirtualTexture>()")
_ns_replace("wiTerrain.cpp" "VirtualTexture& vt = *chunk_data.vt;"
[=[VirtualTexture& vt = *chunk_data.vt;
            const float groundSpan = (chunk_width - 1) * chunk_scale;
            static_cast<GroundChunkVirtualTexture&>(vt).groundFrame = XMFLOAT3(
                chunk_data.position.x - groundSpan * .5f, -chunk_data.position.z - groundSpan * .5f, groundSpan);]=] "const float groundSpan")
_ns_replace("wiTerrain.cpp" "device->AllocateGPU(sizeof(uint) * push.blendmap_layers, cmd);"
[=[device->AllocateGPU(sizeof(uint) * push.blendmap_layers + sizeof(XMFLOAT3), cmd);
                std::memcpy((uint*)mem.data + push.blendmap_layers, &static_cast<const GroundChunkVirtualTexture*>(vt)->groundFrame, sizeof(XMFLOAT3));]=] "push.blendmap_layers + sizeof(XMFLOAT3)")
_ns_replace("shaders/terrainVirtualTextureUpdateCS.hlsl" "ShaderMaterial material = load_material(materialIndex);"
[=[ShaderMaterial material = load_material(materialIndex);
            // A reserved material transform marks project-authored ground. All PBR channels
            // share metres-based sampling across chunk boundaries and virtual texture LODs.
            const bool ground = material.texMulAdd.w == -2718;
            float2 groundUV = uv2;
            float groundRepeat = 1;
            if (ground)
            {
                float3 frame = asfloat(blendmap_buffer.Load3(push.blendmap_buffer_offset + push.blendmap_layers * sizeof(uint)));
                groundRepeat = frame.z / material.texMulAdd.x;
                groundUV = (frame.xy + uv2 * frame.z) / material.texMulAdd.x;
            }]=] "const bool ground =")
_ns_replace("shaders/terrainVirtualTextureUpdateCS.hlsl" "uv2 / overscale, lod)"
    "ground ? groundUV : uv2 / overscale, ground ? lod + log2(groundRepeat) : lod)" "ground ? groundUV")
# Take a snapshot of user rules before scheduling worker jobs.
_ns_replace("wiTerrain.cpp" "wi::jobsystem::Execute(generator->workload, [this](wi::jobsystem::JobArgs a) {"
[=[bool useGroundRules = false, groundHeightEnabled = false;
        XMFLOAT4 groundRules = {};
        if (const auto* meta = scene->metadatas.GetComponent(terrainEntity); meta && meta->float_values.has("renegade.ground.slope_start"))
        {
            useGroundRules = true;
            groundRules = XMFLOAT4(meta->float_values.get("renegade.ground.slope_start"), meta->float_values.get("renegade.ground.slope_full"),
                meta->float_values.get("renegade.ground.height_start"), meta->float_values.get("renegade.ground.height_full"));
            groundHeightEnabled = meta->bool_values.get("renegade.ground.height_enabled");
        }
        wi::jobsystem::Execute(generator->workload, [this,useGroundRules,groundRules,groundHeightEnabled](wi::jobsystem::JobArgs a) {]=] "bool useGroundRules")
_ns_replace("wiTerrain.cpp" "XMFLOAT4 materialBlendWeights(region_base, region_slope, region_low_altitude, region_high_altitude);"
[=[if (useGroundRules)
                        {
                            const float angle = XMConvertToDegrees(std::acos(std::clamp(normal.y,0.f,1.f)));
                            region_slope = smoothstep(groundRules.x, groundRules.y, angle);
                            if (groundHeightEnabled) region_slope = std::max(region_slope, smoothstep(groundRules.z, groundRules.w, height));
                            region_low_altitude = region_high_altitude = 0;
                        }
                        XMFLOAT4 materialBlendWeights(region_base, region_slope, region_low_altitude, region_high_altitude);]=] "if (useGroundRules)")
