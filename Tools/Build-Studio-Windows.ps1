#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string[]]$Configuration = @("Debug", "Release"),

    [switch]$Clean,

    [string]$ArtifactRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    throw "CMake 3.19 or newer is required to build Renegade Studio."
}
$ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $ctestCommand) {
    throw "CTest is required to validate the Renegade bridge."
}

$buildRoot = Join-Path $repositoryRoot "BUILD\renegade"
if ($Clean -and (Test-Path $buildRoot -PathType Container)) {
    Remove-Item -Path $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $ArtifactRoot = Join-Path $repositoryRoot "artifacts\phase2\studio-$timestamp"
}
$ArtifactRoot = [System.IO.Path]::GetFullPath($ArtifactRoot)
New-Item -ItemType Directory -Path $ArtifactRoot -Force | Out-Null

$renegadeCommit = (& git -C $repositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()
$startedUtc = (Get-Date).ToUniversalTime()
$resultPath = Join-Path $ArtifactRoot "build-result.json"
$result = [ordered]@{
    schemaVersion = 1
    status = "IN_PROGRESS"
    startedUtc = $startedUtc.ToString("o")
    completedUtc = $null
    renegadeCommit = $renegadeCommit
    wickedCommit = $wickedCommit
    configurations = @($Configuration)
    platform = "x64"
    targets = @("RenegadeStudio", "RenegadeRuntime", "RenegadeBridgeTests")
    embeddedShaders = $true
    outputs = @()
    error = $null
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    Invoke-RenegadeLoggedCommand `
        -FilePath $cmakeCommand.Source `
        -ArgumentList @(
            "-S", $repositoryRoot,
            "-B", $buildRoot,
            "-A", "x64",
            "-DRENEGADE_EMBED_SHADERS=ON"
        ) `
        -WorkingDirectory $repositoryRoot `
        -LogPath (Join-Path $ArtifactRoot "logs\01-configure.log")

    foreach ($currentConfiguration in $Configuration) {
        Invoke-RenegadeLoggedCommand `
            -FilePath $cmakeCommand.Source `
            -ArgumentList @(
                "--build", $buildRoot,
                "--config", $currentConfiguration,
                "--target", "RenegadeStudio", "RenegadeRuntime", "RenegadeBridgeTests",
                "--parallel"
            ) `
            -WorkingDirectory $repositoryRoot `
            -LogPath (Join-Path $ArtifactRoot "logs\$currentConfiguration\02-build.log")

        Invoke-RenegadeLoggedCommand `
            -FilePath $ctestCommand.Source `
            -ArgumentList @(
                "--test-dir", $buildRoot,
                "-C", $currentConfiguration,
                "--output-on-failure"
            ) `
            -WorkingDirectory $repositoryRoot `
            -LogPath (Join-Path $ArtifactRoot "logs\$currentConfiguration\03-tests.log")

        $studioDirectory = Join-Path $buildRoot "Studio\$currentConfiguration"
        $studioExecutable = Join-Path $studioDirectory "RenegadeStudio.exe"
        $dxCompiler = Join-Path $studioDirectory "dxcompiler.dll"
        $fixtureScene = Join-Path $studioDirectory "Content\cube.wiscene"
        $runtimeDirectory = Join-Path $buildRoot "Runtime\$currentConfiguration"
        $runtimeExecutable = Join-Path $runtimeDirectory "RenegadeRuntime.exe"
        $runtimeDxCompiler = Join-Path $runtimeDirectory "dxcompiler.dll"
        $runtimeFixtureScene = Join-Path $runtimeDirectory "Content\cube.wiscene"

        $packageRoot = Join-Path $ArtifactRoot "packages\RenegadeStudio-$currentConfiguration"
        New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
        Copy-Item -Path $studioExecutable -Destination $packageRoot -Force
        Copy-Item -Path $dxCompiler -Destination $packageRoot -Force
        Copy-Item `
            -Path (Join-Path $studioDirectory "Content") `
            -Destination (Join-Path $packageRoot "Content") `
            -Recurse `
            -Force

        $packageArchive = Join-Path $ArtifactRoot "RenegadeStudio-$currentConfiguration.zip"
        if (Test-Path $packageArchive -PathType Leaf) {
            Remove-Item -Path $packageArchive -Force
        }
        Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $packageArchive

        $runtimePackageRoot = Join-Path $ArtifactRoot "packages\RenegadeRuntime-$currentConfiguration"
        New-Item -ItemType Directory -Path $runtimePackageRoot -Force | Out-Null
        Copy-Item -Path $runtimeExecutable -Destination $runtimePackageRoot -Force
        Copy-Item -Path $runtimeDxCompiler -Destination $runtimePackageRoot -Force
        Copy-Item `
            -Path (Join-Path $runtimeDirectory "Content") `
            -Destination (Join-Path $runtimePackageRoot "Content") `
            -Recurse `
            -Force

        $runtimePackageArchive = Join-Path $ArtifactRoot "RenegadeRuntime-$currentConfiguration.zip"
        if (Test-Path $runtimePackageArchive -PathType Leaf) {
            Remove-Item -Path $runtimePackageArchive -Force
        }
        Compress-Archive `
            -Path (Join-Path $runtimePackageRoot "*") `
            -DestinationPath $runtimePackageArchive

        $result.outputs += [ordered]@{
            configuration = $currentConfiguration
            studio = Get-RenegadeFileRecord -Path $studioExecutable
            dxcompiler = Get-RenegadeFileRecord -Path $dxCompiler
            fixtureScene = Get-RenegadeFileRecord -Path $fixtureScene
            studioPackage = Get-RenegadeFileRecord -Path $packageArchive
            runtime = Get-RenegadeFileRecord -Path $runtimeExecutable
            runtimeDxcompiler = Get-RenegadeFileRecord -Path $runtimeDxCompiler
            runtimeFixtureScene = Get-RenegadeFileRecord -Path $runtimeFixtureScene
            runtimePackage = Get-RenegadeFileRecord -Path $runtimePackageArchive
            bridgeTests = "PASS"
        }
    }

    $stopwatch.Stop()
    $result.status = "PASS"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

    Write-Host ""
    Write-Host "Renegade Phase 2 increment completed successfully."
    Write-Host "Evidence: $resultPath"
}
catch {
    $stopwatch.Stop()
    $result.status = "FAIL"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result.error = $_.Exception.Message
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8
    throw
}
