# Validate real Wicked sky pixel shaders using a SPIR-V-enabled official DXC.
# Default dxc: official Microsoft release v1.9.2609, verified by SHA-256,
# extracted into BUILD/night-sky-check/shader-check/official-dxc/ (not installed).
param([string]$DxcPath)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$shaderRoot = Join-Path $root 'WickedEngine/WickedEngine/shaders'
$out = Join-Path $root 'BUILD/night-sky-check/shader-check/vulkan'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (-not $DxcPath) {
    $DxcPath = Join-Path $root 'BUILD/night-sky-check/shader-check/official-dxc/extracted/bin/x64/dxc.exe'
}
if (-not (Test-Path $DxcPath)) { throw "SPIR-V-enabled DXC missing: $DxcPath" }
# Replicate wiShaderCompiler.cpp / wiGraphicsDevice_Vulkan.h: shifts and sets.
$flags = '-nologo -spirv -fspv-target-env=vulkan1.3 -fvk-use-dx-layout -fvk-use-dx-position-w' +
    ' -fvk-b-shift 0 0 -fvk-t-shift 1000 0 -fvk-u-shift 2000 0 -fvk-s-shift 3000 0' +
    ' -D DESCRIPTOR_SET_BINDLESS_SAMPLER=1' +
    ' -D DESCRIPTOR_SET_BINDLESS_STORAGE_BUFFER=2' +
    ' -D DESCRIPTOR_SET_BINDLESS_UNIFORM_TEXEL_BUFFER=3' +
    ' -D DESCRIPTOR_SET_BINDLESS_SAMPLED_IMAGE=4' +
    ' -D DESCRIPTOR_SET_BINDLESS_STORAGE_IMAGE=5' +
    ' -D DESCRIPTOR_SET_BINDLESS_STORAGE_TEXEL_BUFFER=6' +
    ' -D DESCRIPTOR_SET_BINDLESS_ACCELERATION_STRUCTURE=7 -Vd'
foreach ($name in @('skyPS_dynamic','skyPS_static','envMap_skyPS_dynamic','envMap_skyPS_static','sunPS')) {
    $dest = Join-Path $out "$name.spv"
    $err = Join-Path $out "$name.stderr.txt"
    $args = "$flags -T ps_6_0 -E main -I `"$shaderRoot`" -Fo `"$dest`" `"$shaderRoot/$name.hlsl`""
    $proc = Start-Process -FilePath $DxcPath -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardError $err
    if ($proc.ExitCode -ne 0 -or -not (Test-Path $dest)) {
        Get-Content $err -Tail 25
        throw "Vulkan shader failed: $name (exit $($proc.ExitCode))"
    }
    Write-Output "$name : SPIR-V PASS ($((Get-Item $dest).Length) bytes)"
}
Write-Output 'Five real Wicked sky shader Vulkan variants compiled successfully.'
