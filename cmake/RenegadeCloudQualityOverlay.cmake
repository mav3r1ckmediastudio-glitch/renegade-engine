# Gate 1 core-patch decision: formation-only sampling and reconstruction.
# Native Legacy allocation, quality parameters, serialization and ABI stay intact.
_ns_replace("wiRenderPath3D.cpp" "#include \"wiProfiler.h\"" "#include \"wiProfiler.h\"\n#include \"wiArguments.h\"" "#include \"wiArguments.h\"")
# The native factory's 2:1 render/history ratio is retained with doubled input.
_ns_replace("wiRenderPath3D.cpp"
[=[if (!volumetriccloudResources.texture_cloudRender.IsValid())
			{
				wi::renderer::CreateVolumetricCloudResources(volumetriccloudResources, internalResolution);
			}]=]
[=[// Renegade formation quality: half-size rays, full-size temporal history.
            const float formation = scene->weather.volumetricCloudParameters.layerFirst.padding0;
            const bool formationQuality = formation >= 1 && formation <= 5 &&
                wi::arguments::HasArgument("cloud-reference-quality");
            const XMUINT2 cloudAllocation = formationQuality ?
                XMUINT2(((internalResolution.x + 1) / 2) * 4, ((internalResolution.y + 1) / 2) * 4) :
                internalResolution;
            if (!volumetriccloudResources.texture_cloudRender.IsValid() ||
                volumetriccloudResources.texture_cloudRender.desc.width != cloudAllocation.x / 4 ||
                volumetriccloudResources.texture_cloudRender.desc.height != cloudAllocation.y / 4)
            {
                wi::renderer::CreateVolumetricCloudResources(volumetriccloudResources, cloudAllocation);
                volumetriccloudResources.final_resolution = internalResolution;
            }]=]
"// Renegade formation quality:")
_ns_replace("wiRenderer.cpp"
[=[postprocess.resolution.x = res.final_resolution.x / 4;
	postprocess.resolution.y = res.final_resolution.y / 4;]=]
[=[postprocess.resolution.x = res.texture_cloudRender.desc.width;
    postprocess.resolution.y = res.texture_cloudRender.desc.height;]=]
"postprocess.resolution.x = res.texture_cloudRender.desc.width;")
_ns_replace("wiRenderer.cpp"
[=[postprocess.resolution.x = res.final_resolution.x / 2;
	postprocess.resolution.y = res.final_resolution.y / 2;]=]
[=[postprocess.resolution.x = res.texture_reproject[0].desc.width;
    postprocess.resolution.y = res.texture_reproject[0].desc.height;]=]
"postprocess.resolution.x = res.texture_reproject[0].desc.width;")
_ns_replace("shaders/volumetricCloud_upsamplePS.hlsl"
"const uint2 reprojectionCoord = pixel / 2;"
"const uint2 reprojectionCoord = min(uint2(uv * postprocess.params0.xy), uint2(postprocess.params0.xy) - 1);"
"const uint2 reprojectionCoord = min(")
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
"stepSize = (tMax - tMin) / steps;"
[=[// Renegade sampling: bound distance between samples of the 3D density field.
        // Native scaling yields ~156m per sample even in a shallow layer.
        if (GetWeather().volumetric_clouds.layerFirst.padding0 >= 1 &&
            GetWeather().volumetric_clouds.layerFirst.padding0 <= 5)
            steps = clamp(ceil((tMax - tMin) / (64.0 + 80.0 * saturate((tMin - 3000.0) / 10000.0))), 1.0, 384.0);
        stepSize = (tMax - tMin) / steps;]=]
"// Renegade sampling:")
# A weather potential is not a conservative distance field. New formations
# take one bounded step in empty regions; Legacy retains its native skip.
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
[=[i += GetWeather().volumetric_clouds.bigStepMarch - 1;
			t += GetWeather().volumetric_clouds.bigStepMarch * stepSize;]=]
[=[float emptyStep = layerParametersFirst.layer.padding0 >= 1 &&
                layerParametersFirst.layer.padding0 <= 5 ? 1.0 : GetWeather().volumetric_clouds.bigStepMarch;
            i += emptyStep - 1;
            t += emptyStep * stepSize;]=]
"float emptyStep =")

_ns_replace("wiRenderPath3D.cpp"
"const bool formationQuality = formation >= 1 && formation <= 5;"
[=[const bool formationQuality = formation >= 1 && formation <= 5 &&
                wi::arguments::HasArgument("cloud-reference-quality");]=]
[=[wi::arguments::HasArgument("cloud-reference-quality")]=])

_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
"const float cloudTopRadius = planetRadius + GetWeather().volumetric_clouds.cloudStartHeight + GetWeather().volumetric_clouds.cloudThickness;"
"const float cloudTopRadius = RenegadeCloudMarchTopRadius(planetRadius + GetWeather().volumetric_clouds.cloudStartHeight + GetWeather().volumetric_clouds.cloudThickness);"
"const float cloudTopRadius = RenegadeCloudMarchTopRadius(")


# Migration from the rejected signed-distance body experiment. A continuous
# weather potential is not a conservative distance field; never sphere-trace it.
file(READ "${_ns_wicked}/shaders/volumetricCloud_renderCS.hlsl" _cloud_march_source)
string(FIND "${_cloud_march_source}" "floor(-weatherDataFirst.r" _cloud_old_skip)
if(NOT _cloud_old_skip EQUAL -1)
    _ns_replace("shaders/volumetricCloud_renderCS.hlsl"
[=[if (layerParametersFirst.layer.padding0 >= 1 && layerParametersFirst.layer.padding0 <= 5 &&
                layerParametersSecond.layer.coverageAmount <= 0 && layerParametersSecond.layer.coverageMinimum <= 0)
                emptyStep = max(1.0, floor(-weatherDataFirst.r / (stepSize * 2.0)));
            i += emptyStep - 1;]=]
[=[// Continuous weather potential: bounded single-step empty traversal.
            i += emptyStep - 1;]=]
"// Continuous weather potential:")
endif()

# Idempotent migration for already-instrumented worktrees: the original
# _ns_replace insert returns early when its marker exists. Keep the generated
# Wicked shader synchronized with the authored adaptive marching algorithm.
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
[=[steps = clamp(ceil((tMax - tMin) / 48.0), 1.0, 384.0);]=]
[=[// Adaptive world-space sampling: preserve 48-m near-field density edges.
            // Distant rays permit wider steps, where perspective and temporal
            // integration resolve fine-scale structure without uniform 384 steps.
            steps = clamp(ceil((tMax - tMin) / (64.0 + 80.0 * saturate((tMin - 3000.0) / 10000.0))), 1.0, 384.0);]=]
"// Adaptive world-space sampling")
file(READ "${_ns_wicked}/shaders/volumetricCloud_renderCS.hlsl" _adaptive_march_compiled)
string(FIND "${_adaptive_march_compiled}" "80.0 * saturate((tMin - 3000.0) / 10000.0)" _adaptive_march_found)
if(_adaptive_march_found EQUAL -1)
    message(FATAL_ERROR "Gate 1 adaptive cloud marching overlay did not synchronize generated shader")
endif()

# Migrate the preceding 48m near-field candidate to the 64m near-field
# performance experiment in an already-patched Wicked checkout.
_ns_replace("shaders/volumetricCloud_renderCS.hlsl"
[=[steps = clamp(ceil((tMax - tMin) / (48.0 + 80.0 * saturate((tMin - 3000.0) / 10000.0))), 1.0, 384.0);]=]
[=[steps = clamp(ceil((tMax - tMin) / (64.0 + 80.0 * saturate((tMin - 3000.0) / 10000.0))), 1.0, 384.0);]=]
"64.0 + 80.0 * saturate(")
