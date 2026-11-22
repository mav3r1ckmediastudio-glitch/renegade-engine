# Renegade palette hair uses independently scattered strand IDs. Rotate each group
# of N IDs so every selected atlas frame occurs once per complete group.
_ns_replace("shaders/hairparticle_simulateCS.hlsl"
    "currentFrame = uint(noise_gradient_3D(position * xHairUniformity) * 1000) % xHairAtlasRectCount;"
[=[if (xHairUniformity == -1)
        {
            RNG paletteRng;
            paletteRng.init(uint2(xHairRandomSeed, DTid.x / xHairAtlasRectCount), 2);
            currentFrame = (DTid.x % xHairAtlasRectCount + paletteRng.next_uint(xHairAtlasRectCount)) % xHairAtlasRectCount;
        }
        else
        {
            currentFrame = uint(noise_gradient_3D(position * xHairUniformity) * 1000) % xHairAtlasRectCount;
        }]=] "RNG paletteRng;")

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

# A brush edit can invalidate the *next* terrain update after the CPU has
# already prepared this frame. Finish queued work using its live allocation.
_ns_replace("wiTerrain.cpp" "vt->resolution >> request.lod"
    "(vt->residency ? vt->residency->resolution : min_virtual_resolution) >> request.lod"
    "min_virtual_resolution) >> request.lod")
_ns_replace("wiTerrain.cpp" "clear_residencies[vt->resolution]"
    "clear_residencies[vt->residency->resolution]" "clear_residencies[vt->residency->resolution]")
_ns_replace("wiTerrain.cpp" "clear.resolution != vt->resolution"
    "clear.resolution != vt->residency->resolution" "clear.resolution != vt->residency->resolution")
_ns_replace("wiTerrain.cpp" "clear.init(vt->resolution)"
    "clear.init(vt->residency->resolution)" "clear.init(vt->residency->resolution)")
# Undo can remove a material after the old blendmap was prepared for this frame.
_ns_replace("wiTerrain.cpp" "const uint baseMaterialCount = push.blendmap_layers - splineMaterialCount;"
[=[if (push.blendmap_layers < splineMaterialCount || push.blendmap_layers - splineMaterialCount > materialEntities.size())
                    continue;
                const uint baseMaterialCount = push.blendmap_layers - splineMaterialCount;]=]
    "push.blendmap_layers - splineMaterialCount > materialEntities.size()")

# Upload the dimensions of the actual source map for each layer and PBR channel.
# Divergent GetDimensions on bindless terrain textures removes the WARP device.
# Keep the same UV and mip calculations for both authored and legacy materials.
_ns_replace("wiTerrain.cpp"
[=[device->AllocateGPU(sizeof(uint) * push.blendmap_layers + sizeof(XMFLOAT3), cmd);]=]
[=[device->AllocateGPU(sizeof(uint) * push.blendmap_layers + sizeof(XMFLOAT3) + sizeof(XMUINT2) * push.blendmap_layers, cmd);]=] "sizeof(XMUINT2) * push.blendmap_layers")
_ns_replace("wiTerrain.cpp"
[=[				for (uint i = 0; i < splineMaterialCount; ++i)
				{
					const Entity entity = splineMaterialEntities[i];
					const uint material_index = (uint)scene->materials.GetIndex(entity);
					std::memcpy((uint*)mem.data + baseMaterialCount + i, &material_index, sizeof(uint)); // force memcpy to avoid uncached read from GPU pointer!
				}
				for (uint i = 0; i < baseMaterialCount; ++i)
				{
					const Entity entity = materialEntities[i];
					const uint material_index = (uint)scene->materials.GetIndex(entity);
					std::memcpy((uint*)mem.data + i, &material_index, sizeof(uint)); // force memcpy to avoid uncached read from GPU pointer!
				}]=]
[=[                // GPU upload memory is write-combined. Only write to it.
                auto* indices = static_cast<uint*>(mem.data);
                auto* sizes = reinterpret_cast<uint8_t*>(indices + push.blendmap_layers) + sizeof(XMFLOAT3);
                for (uint layer = 0; layer < push.blendmap_layers; ++layer)
                {
                    const Entity entity = layer < baseMaterialCount ? materialEntities[layer]
                        : splineMaterialEntities[layer - baseMaterialCount];
                    const uint index = uint(scene->materials.GetIndex(entity));
                    std::memcpy(indices + layer, &index, sizeof(index));
                    XMUINT2 dimensions = {1, 1};
                    if (index < scene->materials.GetCount())
                    {
                        const auto& source = scene->materials[index].textures[map_type].resource;
                        if (source.IsValid() && source.GetTexture().IsValid())
                        {
                            const auto& desc = source.GetTexture().desc;
                            dimensions = {desc.width, desc.height};
                        }
                    }
                    std::memcpy(sizes + layer * sizeof(dimensions), &dimensions, sizeof(dimensions));
                }]=] "std::memcpy(sizes + layer")
_ns_replace("shaders/terrainVirtualTextureUpdateCS.hlsl"
[=[float2 dim = 0;
				tex.GetDimensions(dim.x, dim.y);]=]
[=[uint dimensionOffset = push.blendmap_buffer_offset + push.blendmap_layers * sizeof(uint)
                    + sizeof(float3) + blendmap_index * sizeof(uint2);
                float2 dim = float2(blendmap_buffer.Load2(dimensionOffset));]=] "blendmap_buffer.Load2(dimensionOffset)")
