# Validate the isolated Night Sky preview BEFORE starting Studio.
# This checks the precise regression that previously blacked out the day sky.
$ErrorActionPreference = 'Stop'
try {
    $root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
    $studio = Join-Path $root 'BUILD/night-sky-check/Studio/Release/RenegadeStudioMilkyWayColourPreview.exe'
    $shaderDump = Join-Path $root 'BUILD/night-sky-check/WickedEngine/WickedEngine/wiShaderDump.h'
    $interop = Join-Path $root 'WickedEngine/WickedEngine/shaders/ShaderInterop_Weather.h'
    $testExe = Join-Path $root 'BUILD/night-sky-check/Tests/Release/RenegadeNightSkyTests.exe'
    $project = Join-Path $env:USERPROFILE 'Downloads/Renegade Night Sky Preview/Renegade Night Sky Visual Test/Renegade Night Sky Visual Test.renegade'
    $scene = Join-Path $env:USERPROFILE 'Downloads/Renegade Night Sky Preview/Renegade Night Sky Visual Test/Content/Scenes/Main.wiscene'
    foreach ($required in @($studio,$shaderDump,$interop,$testExe,$project,$scene)) {
        if (-not (Test-Path $required -PathType Leaf)) { throw "Missing file: $required" }
    }
    if ((Get-Item $studio).LastWriteTimeUtc -le (Get-Item $shaderDump).LastWriteTimeUtc) {
        throw 'Studio has not been rebuilt after its shaders'
    }
    if (-not ([IO.File]::ReadAllText($interop)).Contains('float night_alignment_padding;')) {
        throw 'Unsafe GPU weather field layout: padding fix is absent'
    }
    & $testExe --verify-saved-scene $scene
    if ($LASTEXITCODE -ne 0) { throw 'Preview project failed the saved WISCENE test' }
    Write-Host 'Night Sky ABI + scene-file safety checks passed.'
    Write-Host "Use Open Project to select: $project"
    exit 0
}
catch {
    Write-Error "Night Sky preview NOT SAFE to launch: $($_.Exception.Message)"
    exit 1
}
