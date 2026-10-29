# Bounded core-patch decision: docs/CLOUD_FORMATION_GATE1_BODY_CACHE_DECISION_2026-10-29.md
foreach(_body_file renegadeCloudBodyTemplate.hlsli renegadeCloudFormationVolumeCS.hlsl)
    configure_file("${CMAKE_SOURCE_DIR}/Tools/night_sky/${_body_file}"
        "${_ns_wicked}/shaders/${_body_file}" COPYONLY)
endforeach()
_ns_replace("offlineshadercompiler.cpp"
[=[{"volumetricCloud_shapenoiseCS", wi::graphics::ShaderStage::CS },]=]
[=[{"volumetricCloud_shapenoiseCS", wi::graphics::ShaderStage::CS },
    {"renegadeCloudFormationVolumeCS", wi::graphics::ShaderStage::CS },]=]
[=[{"renegadeCloudFormationVolumeCS",]=])
_ns_replace("wiRenderer.cpp" "#include <mutex>" "#include <mutex>\n#include <cmath>" "#include <cmath>")
_ns_replace("wiRenderer.cpp"
"Texture texture_weatherMap;"
[=[Texture texture_weatherMap;
// Renegade cached 3D bodies and immutable site attributes:
Texture texture_renegadeCloudBodies;
GPUBuffer buffer_renegadeCloudSites;
Shader shader_renegadeCloudBodies;
bool renegadeCloudBodiesReady = false;
struct RenegadeCachedCloudSite { XMFLOAT4 attributes; XMFLOAT4 variation; };
static_assert(sizeof(RenegadeCachedCloudSite) == 32);
void BindRenegadeCloudFormationResources(CommandList cmd)
{
    if (texture_renegadeCloudBodies.IsValid())
        device->BindResource(&texture_renegadeCloudBodies, 5, cmd);
    if (buffer_renegadeCloudSites.IsValid())
        device->BindResource(&buffer_renegadeCloudSites, 6, cmd);
}]=]
"// Renegade cached 3D bodies")
_ns_replace("wiRenderer.cpp"
[=[LoadShader(ShaderStage::CS, shaders[CSTYPE_POSTPROCESS_VOLUMETRICCLOUDS_SHAPENOISE], "volumetricCloud_shapenoiseCS.cso"); });]=]
[=[LoadShader(ShaderStage::CS, shaders[CSTYPE_POSTPROCESS_VOLUMETRICCLOUDS_SHAPENOISE], "volumetricCloud_shapenoiseCS.cso"); });
    wi::jobsystem::Execute(ctx, [](wi::jobsystem::JobArgs args) {
        LoadShader(ShaderStage::CS, shader_renegadeCloudBodies, "renegadeCloudFormationVolumeCS.cso");
    });]=]
[=[LoadShader(ShaderStage::CS, shader_renegadeCloudBodies,]=])
_ns_replace("wiRenderer.cpp" "void LoadShaders()\n{"
"void LoadShaders()\n{\n    renegadeCloudBodiesReady = false;"
"void LoadShaders()\n{\n    renegadeCloudBodiesReady = false;")
_ns_replace("wiRenderer.cpp"
"// Fill Entity Array with decals + envprobes + lights in the frustum:"
[=[// Renegade derived cloud resources are allocated only for new Cumulus.
    if (scene.weather.IsVolumetricClouds() &&
        scene.weather.volumetricCloudParameters.layerFirst.padding0 == 1 &&
        !texture_renegadeCloudBodies.IsValid())
    {
        TextureDesc body;
        body.type = TextureDesc::Type::TEXTURE_3D;
        body.width = body.height = body.depth = 128;
        body.mip_levels = 7;
        body.format = Format::R16G16B16A16_FLOAT;
        body.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
        body.layout = ResourceState::SHADER_RESOURCE_COMPUTE;
        device->CreateTexture(&body, nullptr, &texture_renegadeCloudBodies);
        device->SetName(&texture_renegadeCloudBodies, "RenegadeCloudBodyVolume");
        for (uint32_t mip = 0; mip < body.mip_levels; ++mip)
        {
            device->CreateSubresource(&texture_renegadeCloudBodies, SubresourceType::SRV, 0, 1, mip, 1);
            device->CreateSubresource(&texture_renegadeCloudBodies, SubresourceType::UAV, 0, 1, mip, 1);
        }
        const auto mix = [](uint32_t v) {
            v ^= v >> 16; v *= 0x7feb352d; v ^= v >> 15; v *= 0x846ca68b; v ^= v >> 16; return v;
        };
        const auto corner = [&](int x, int z) {
            return float(mix(uint32_t(x) * 0x9e3779b9u ^ uint32_t(z) * 0x85ebca6bu) & 0x00ffffffu) / 16777216.0f;
        };
        wi::vector<RenegadeCachedCloudSite> sites(64 * 64);
        for (int iz = 0; iz < 64; ++iz) for (int ix = 0; ix < 64; ++ix)
        {
            const int x = ix < 32 ? ix : ix - 64, z = iz < 32 ? iz : iz - 64;
            const int rx = int(std::floor(float(x) / 4)), rz = int(std::floor(float(z) / 4));
            const uint32_t seed = mix(uint32_t(rx) * 0x9e3779b9u ^ uint32_t(rz) * 0x85ebca6bu);
            // Hierarchical 4x4 rank: preserve exactly 16 unique ranks in
            // every 12-km cell but admit spatially coherent 2x2 groups.
            // This produces whole-body clear lanes at partial coverage;
            // no 2D shape mask, enlarged footprint or camera-dependent rule.
            const uint32_t localX = uint32_t(x - rx * 4);
            const uint32_t localZ = uint32_t(z - rz * 4);
            const uint32_t quadrant = (localX >> 1) | ((localZ >> 1) << 1);
            const uint32_t child = (localX & 1u) | ((localZ & 1u) << 1);
            uint32_t quadrantOrder[4] = {0u, 1u, 2u, 3u};
            uint32_t rankSeed = seed;
            for (int k = 3; k > 0; --k)
            {
                rankSeed = mix(rankSeed + 0x9e3779b9u);
                const uint32_t index = rankSeed % uint32_t(k + 1);
                const uint32_t temp = quadrantOrder[k];
                quadrantOrder[k] = quadrantOrder[index];
                quadrantOrder[index] = temp;
            }
            uint32_t quadrantRank = 0;
            for (uint32_t k = 0; k < 4; ++k)
                if (quadrantOrder[k] == quadrant) quadrantRank = k;
            const uint32_t intraSeed = mix(rankSeed ^ (quadrant * 0x85ebca6bu));
            const uint32_t intraOdd = ((intraSeed >> 8) & 3u) | 1u;
            // Hybrid clumping: alternate deterministic clustered and dispersed
            // 4x4 regions, limiting wide-angle under/overfill in held-out views.
            const uint32_t clusterRank = quadrantRank * 4u +
                ((child * intraOdd + (intraSeed & 3u)) & 3u);
            const uint32_t dispersedOdd = ((seed >> 8) & 15u) | 1u;
            const uint32_t dispersedRank =
                ((localX + localZ * 4u) * dispersedOdd + (seed & 15u)) & 15u;
            const uint32_t rank = ((seed >> 24) & 1u) ? clusterRank : dispersedRank;
            const float rotation = corner(x + 47, z - 29) * 6.28318530718f;
            auto& site = sites[ix + iz * 64];
            // Decorrelated cell displacement breaks the visible 3-km rows.
            // The 4x4 rotating candidate window conservatively encloses the
            // maximum body radius even at the extreme +/-0.45-cell offset.
            site.attributes = XMFLOAT4((corner(x, z) - .5f) * .9f,
                (corner(x + 37, z - 71) - .5f) * .9f, (rank + .5f) / 16.0f, corner(x + 113, z - 61));
            site.variation = XMFLOAT4(std::cos(rotation), std::sin(rotation),
                corner(x - 19, z + 83), corner(x + 101, z + 7));
        }
        GPUBufferDesc desc;
        desc.size = sites.size() * sizeof(RenegadeCachedCloudSite);
        desc.stride = sizeof(RenegadeCachedCloudSite);
        desc.bind_flags = BindFlag::SHADER_RESOURCE;
        desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
        device->CreateBuffer(&desc, sites.data(), &buffer_renegadeCloudSites);
        device->SetName(&buffer_renegadeCloudSites, "RenegadeCloudSites");
        renegadeCloudBodiesReady = false;
    }

    // Fill Entity Array with decals + envprobes + lights in the frustum:]=]
"// Renegade derived cloud resources")
# Upgrade the initial experimental overlay whose condition also occurred in
# CPU-side shadow-data setup. Remove both old insertions, then patch only
# the GPU block identified by its weather-map bindings.
file(READ "${_ns_wicked}/wiRenderer.cpp" _body_cpp_before)
set(_body_old_generation [=[// Renegade immutable body-volume generation, after native noise initialization.
    if (texture_renegadeCloudBodies.IsValid() && !renegadeCloudBodiesReady &&
        vis.scene->weather.IsVolumetricClouds() &&
        vis.scene->weather.volumetricCloudParameters.layerFirst.padding0 == 1)
    {
        device->EventBegin("Renegade Cloud Body Volume", cmd);
        BindCommonResources(cmd);
        auto barrier = GPUBarrier::Image(&texture_renegadeCloudBodies,
            texture_renegadeCloudBodies.desc.layout, ResourceState::UNORDERED_ACCESS);
        device->Barrier(&barrier, 1, cmd);
        device->BindComputeShader(&shader_renegadeCloudBodies, cmd);
        device->BindResource(&texture_detailNoise, 0, cmd);
        device->BindUAV(&texture_renegadeCloudBodies, 0, cmd);
        device->Dispatch(16, 16, 16, cmd);
        barrier = GPUBarrier::Image(&texture_renegadeCloudBodies,
            ResourceState::UNORDERED_ACCESS, texture_renegadeCloudBodies.desc.layout);
        device->Barrier(&barrier, 1, cmd);
        GenerateMipChain(texture_renegadeCloudBodies, MIPGENFILTER_LINEAR, cmd);
        renegadeCloudBodiesReady = true;
        device->EventEnd(cmd);
    }
    if (vis.scene->weather.IsVolumetricClouds() && vis.scene->weather.IsVolumetricCloudsCastShadow())]=])
string(REPLACE "${_body_old_generation}" [=[if (vis.scene->weather.IsVolumetricClouds() && vis.scene->weather.IsVolumetricCloudsCastShadow())]=] _body_cpp_after "${_body_cpp_before}")
if(NOT _body_cpp_before STREQUAL _body_cpp_after)
    file(WRITE "${_ns_wicked}/wiRenderer.cpp" "${_body_cpp_after}")
endif()
_ns_replace("wiRenderer.cpp"
[=[if (vis.scene->weather.IsVolumetricClouds() && vis.scene->weather.IsVolumetricCloudsCastShadow())
	{
		const Texture* weatherMapFirst]=]
[=[// Renegade body-volume GPU generation, after native noise initialization.
    if (texture_renegadeCloudBodies.IsValid() && !renegadeCloudBodiesReady &&
        vis.scene->weather.IsVolumetricClouds() &&
        vis.scene->weather.volumetricCloudParameters.layerFirst.padding0 == 1)
    {
        device->EventBegin("Renegade Cloud Body Volume", cmd);
        BindCommonResources(cmd);
        auto barrier = GPUBarrier::Image(&texture_renegadeCloudBodies,
            texture_renegadeCloudBodies.desc.layout, ResourceState::UNORDERED_ACCESS);
        device->Barrier(&barrier, 1, cmd);
        device->BindComputeShader(&shader_renegadeCloudBodies, cmd);
        device->BindResource(&texture_detailNoise, 0, cmd);
        device->BindUAV(&texture_renegadeCloudBodies, 0, cmd);
        device->Dispatch(16, 16, 16, cmd);
        barrier = GPUBarrier::Image(&texture_renegadeCloudBodies,
            ResourceState::UNORDERED_ACCESS, texture_renegadeCloudBodies.desc.layout);
        device->Barrier(&barrier, 1, cmd);
        GenerateMipChain(texture_renegadeCloudBodies, MIPGENFILTER_LINEAR, cmd);
        renegadeCloudBodiesReady = true;
        device->EventEnd(cmd);
    }
    if (vis.scene->weather.IsVolumetricClouds() && vis.scene->weather.IsVolumetricCloudsCastShadow())
	{
		const Texture* weatherMapFirst]=]
"// Renegade body-volume GPU generation")
# Same binding seam for camera, terrain shadows, environment capture and proofs.
_ns_replace("wiRenderer.cpp"
"device->BindResource(&texture_curlNoise, 2, cmd);"
"device->BindResource(&texture_curlNoise, 2, cmd);\n    BindRenegadeCloudFormationResources(cmd);"
"    BindRenegadeCloudFormationResources(cmd);")


# Upgrade only the authored Cumulus dispatch from the earlier volume experiment.
file(READ "${_ns_wicked}/shaders/volumetricCloudsHF.hlsli" _wind_hf_before)
string(REPLACE [=[            p + parameters.windOffset, heightFraction, weatherData.rg, lod, sampleDetail);]=] [=[            p + float3(parameters.windOffset.x, 0, parameters.windOffset.z),
            heightFraction, weatherData.rg, lod, sampleDetail, parameters.windOffset.y);]=] _wind_hf_after "${_wind_hf_before}")
string(REPLACE "return RenegadeWholeCloudWeather(p + parameters.windOffset);"
    "return RenegadeWholeCloudWeather(p, parameters.windOffset);" _wind_hf_after "${_wind_hf_after}")
if(NOT _wind_hf_before STREQUAL _wind_hf_after)
    file(WRITE "${_ns_wicked}/shaders/volumetricCloudsHF.hlsli" "${_wind_hf_after}")
endif()

# Cumulus weather carries conservative complete-body bounds and candidate IDs.
_ns_replace("shaders/volumetricCloudsHF.hlsli"
[=[float3 SampleWeather(Texture2D<float4> texture_weatherMap, float3 p, float heightFraction, LayerParameters parameters)
{]=]
[=[float3 SampleWeather(Texture2D<float4> texture_weatherMap, float3 p, float heightFraction, LayerParameters parameters)
{
    if (parameters.layer.padding0 == 1)
        return RenegadeWholeCloudWeather(p, parameters.windOffset);]=]
"return RenegadeWholeCloudWeather(p, parameters.windOffset);")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
[=[float emptyStep = layerParametersFirst.layer.padding0 >= 1 &&
                layerParametersFirst.layer.padding0 <= 5 ? 1.0 : GetWeather().volumetric_clouds.bigStepMarch;]=]
[=[float emptyStep = layerParametersFirst.layer.padding0 >= 1 &&
                layerParametersFirst.layer.padding0 <= 5 ? 1.0 : GetWeather().volumetric_clouds.bigStepMarch;
            // Renegade conservative whole-body bounds: the spherical-height
            // coordinate map has a norm below 2. Never skip a secondary layer.
            if (layerParametersFirst.layer.padding0 == 1 &&
                layerParametersSecond.layer.coverageAmount <= 0 &&
                layerParametersSecond.layer.coverageMinimum <= 0)
                emptyStep = max(1.0, floor((-weatherDataFirst.r) / (2.0 * stepSize)));]=]
"// Renegade conservative whole-body bounds:")

include("${CMAKE_CURRENT_LIST_DIR}/RenegadeCloudReferenceOverlay.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/RenegadeCloudFieldMetricOverlay.cmake")

# Upgrade an already-patched isolated Wicked checkout. The initial resource
# insertion above is intentionally idempotent: changing its authored payload
# does not replace a prior injection. Explicitly migrate existing site arrays
# so reconfigure, clean builds and incremental builds share identical layout.
_ns_replace("wiRenderer.cpp"
[=[            const uint32_t odd = ((seed >> 8) & 15u) | 1u;
            const uint32_t rank = (uint32_t(x - rx * 4 + (z - rz * 4) * 4) * odd + (seed & 15u)) & 15u;]=]
[=[            // Hierarchical 4x4 rank: admit complete groups before adjacent
            // gaps fill; every 12-km tile still has unique ranks 0..15.
            const uint32_t localX = uint32_t(x - rx * 4);
            const uint32_t localZ = uint32_t(z - rz * 4);
            const uint32_t quadrant = (localX >> 1) | ((localZ >> 1) << 1);
            const uint32_t child = (localX & 1u) | ((localZ & 1u) << 1);
            uint32_t quadrantOrder[4] = {0u, 1u, 2u, 3u};
            uint32_t rankSeed = seed;
            for (int k = 3; k > 0; --k)
            {
                rankSeed = mix(rankSeed + 0x9e3779b9u);
                const uint32_t index = rankSeed % uint32_t(k + 1);
                const uint32_t temp = quadrantOrder[k];
                quadrantOrder[k] = quadrantOrder[index];
                quadrantOrder[index] = temp;
            }
            uint32_t quadrantRank = 0;
            for (uint32_t k = 0; k < 4; ++k)
                if (quadrantOrder[k] == quadrant) quadrantRank = k;
            const uint32_t intraSeed = mix(rankSeed ^ (quadrant * 0x85ebca6bu));
            const uint32_t intraOdd = ((intraSeed >> 8) & 3u) | 1u;
            // Hybrid clumping: deterministic clustered/dispersed 4x4 tiles.
            const uint32_t clusterRank = quadrantRank * 4u +
                ((child * intraOdd + (intraSeed & 3u)) & 3u);
            const uint32_t dispersedOdd = ((seed >> 8) & 15u) | 1u;
            const uint32_t dispersedRank =
                ((localX + localZ * 4u) * dispersedOdd + (seed & 15u)) & 15u;
            const uint32_t rank = ((seed >> 24) & 1u) ? clusterRank : dispersedRank;]=]
"// Hierarchical 4x4 rank:")
_ns_replace("wiRenderer.cpp"
[=[            site.attributes = XMFLOAT4((corner(x, z) - .5f) * .4f,
                (corner(x + 37, z - 71) - .5f) * .4f, (rank + .5f) / 16.0f, corner(x + 113, z - 61));]=]
[=[            // Decorrelated cell displacement; the rotating 4x4 window
            // includes maximum ±0.45 cell offset plus full volume support.
            site.attributes = XMFLOAT4((corner(x, z) - .5f) * .9f,
                (corner(x + 37, z - 71) - .5f) * .9f, (rank + .5f) / 16.0f, corner(x + 113, z - 61));]=]
"// Decorrelated cell displacement; the rotating 4x4 window")

# Migrate the already-generated coherent-only experiment to the bounded
# hybrid before regression checks. This step is idempotent on fresh trees.
_ns_replace("wiRenderer.cpp"
[=[            const uint32_t rank = quadrantRank * 4u +
                ((child * intraOdd + (intraSeed & 3u)) & 3u);]=]
[=[            // Hybrid clumping: half locally clustered, half dispersed.
            const uint32_t clusterRank = quadrantRank * 4u +
                ((child * intraOdd + (intraSeed & 3u)) & 3u);
            const uint32_t dispersedOdd = ((seed >> 8) & 15u) | 1u;
            const uint32_t dispersedRank =
                ((localX + localZ * 4u) * dispersedOdd + (seed & 15u)) & 15u;
            const uint32_t rank = ((seed >> 24) & 1u) ? clusterRank : dispersedRank;]=]
"// Hybrid clumping:")

# Gate-1 regression guard: do not silently run old generated site placement
# when an already-instrumented fork skips an initial _ns_replace insertion.
file(READ "${_ns_wicked}/wiRenderer.cpp" _cluster_generated_cpp)
foreach(_site_needle
    "Hierarchical 4x4 rank:"
    "Hybrid clumping:"
    "Decorrelated cell displacement; the rotating 4x4 window"
    "site.attributes = XMFLOAT4((corner(x, z) - .5f) * .9f,")
    string(FIND "${_cluster_generated_cpp}" "${_site_needle}" _site_index)
    if(_site_index EQUAL -1)
        message(FATAL_ERROR "Gate 1 cloud site overlay not synchronized: ${_site_needle}")
    endif()
endforeach()
string(FIND "${_cluster_generated_cpp}"
    "site.attributes = XMFLOAT4((corner(x, z) - .5f) * .4f," _stale_jitter)
if(NOT _stale_jitter EQUAL -1)
    message(FATAL_ERROR "Gate 1 old cloud jitter remains in generated wiRenderer.cpp")
endif()
