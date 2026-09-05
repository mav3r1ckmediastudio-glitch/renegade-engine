#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string[]]$Configuration = @("Debug", "Release"),

    [switch]$Clean,

    [switch]$SkipShaderCompilation,

    [switch]$SkipTests,

    [string]$ArtifactRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedRoot = Join-Path $repositoryRoot "WickedEngine"
$solutionPath = Join-Path $wickedRoot "WickedEngine.sln"
$testsProjectPath = Join-Path $wickedRoot "Samples\Tests\Tests.vcxproj"
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot
$msbuildPath = Get-RenegadeMSBuildPath

if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $ArtifactRoot = Join-Path $repositoryRoot "artifacts\phase1\build-$timestamp"
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
    shaderCompilation = -not $SkipShaderCompilation
    testsBuilt = -not $SkipTests
    outputs = @()
    error = $null
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    if (-not $SkipShaderCompilation) {
        $shaderConfiguration = if ($Configuration -contains "Release") {
            "Release"
        }
        else {
            $Configuration[0]
        }

        $shaderLog = Join-Path $ArtifactRoot "logs\$shaderConfiguration\01-offline-shader-compiler.log"
        Invoke-RenegadeLoggedCommand `
            -FilePath $msbuildPath `
            -ArgumentList @(
                $solutionPath,
                "/t:OfflineShaderCompiler",
                "/m",
                "/p:Configuration=$shaderConfiguration",
                "/p:Platform=x64"
            ) `
            -WorkingDirectory $wickedRoot `
            -LogPath $shaderLog

        $shaderCompilerPath = Join-Path $wickedRoot "BUILD\x64\$shaderConfiguration\OfflineShaderCompiler\OfflineShaderCompiler.exe"
        if (-not (Test-Path $shaderCompilerPath -PathType Leaf)) {
            throw "OfflineShaderCompiler did not produce '$shaderCompilerPath'."
        }

        $shaderDumpLog = Join-Path $ArtifactRoot "logs\$shaderConfiguration\02-generate-shader-dump.log"
        Invoke-RenegadeLoggedCommand `
            -FilePath $shaderCompilerPath `
            -ArgumentList @("hlsl6", "spirv", "shaderdump", "strip_reflection") `
            -WorkingDirectory (Join-Path $wickedRoot "WickedEngine") `
            -LogPath $shaderDumpLog
    }

    foreach ($currentConfiguration in $Configuration) {
        $configurationLogRoot = Join-Path $ArtifactRoot "logs\$currentConfiguration"

        if ($Clean) {
            Invoke-RenegadeLoggedCommand `
                -FilePath $msbuildPath `
                -ArgumentList @(
                    $solutionPath,
                    "/t:Clean",
                    "/m",
                    "/p:Configuration=$currentConfiguration",
                    "/p:Platform=x64"
                ) `
                -WorkingDirectory $wickedRoot `
                -LogPath (Join-Path $configurationLogRoot "03-clean.log")
        }

        Invoke-RenegadeLoggedCommand `
            -FilePath $msbuildPath `
            -ArgumentList @(
                $solutionPath,
                "/t:Editor_Windows",
                "/m",
                "/p:Configuration=$currentConfiguration",
                "/p:Platform=x64"
            ) `
            -WorkingDirectory $wickedRoot `
            -LogPath (Join-Path $configurationLogRoot "04-editor.log")

        if (-not $SkipTests) {
            Invoke-RenegadeLoggedCommand `
                -FilePath $msbuildPath `
                -ArgumentList @(
                    $testsProjectPath,
                    "/m",
                    "/p:Configuration=$currentConfiguration",
                    "/p:Platform=x64",
                    "/p:SolutionDir=$wickedRoot\"
                ) `
                -WorkingDirectory $wickedRoot `
                -LogPath (Join-Path $configurationLogRoot "05-tests.log")
        }

        $editorOutput = Join-Path $wickedRoot "BUILD\x64\$currentConfiguration\Editor_Windows\Editor_Windows.exe"
        $outputRecord = [ordered]@{
            configuration = $currentConfiguration
            editor = Get-RenegadeFileRecord -Path $editorOutput
            tests = $null
            package = $null
        }

        if (-not $SkipTests) {
            $testsOutput = Join-Path $wickedRoot "BUILD\x64\$currentConfiguration\Tests\Tests.exe"
            $outputRecord.tests = Get-RenegadeFileRecord -Path $testsOutput
        }

        $packageRoot = Join-Path $ArtifactRoot "packages\WickedEditor-$currentConfiguration"
        New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
        Copy-Item -Path $editorOutput -Destination $packageRoot -Force

        $builtDxCompiler = Join-Path (Split-Path -Parent $editorOutput) "dxcompiler.dll"
        $sourceDxCompiler = Join-Path $wickedRoot "WickedEngine\dxcompiler.dll"
        if (Test-Path $builtDxCompiler -PathType Leaf) {
            Copy-Item -Path $builtDxCompiler -Destination $packageRoot -Force
        }
        elseif (Test-Path $sourceDxCompiler -PathType Leaf) {
            Copy-Item -Path $sourceDxCompiler -Destination $packageRoot -Force
        }

        $editorAssets = Join-Path $wickedRoot "Editor"
        foreach ($fileName in @("config.ini", "startup.lua")) {
            Copy-Item -Path (Join-Path $editorAssets $fileName) -Destination $packageRoot -Force
        }
        foreach ($directoryName in @("fonts", "languages")) {
            Copy-Item `
                -Path (Join-Path $editorAssets $directoryName) `
                -Destination (Join-Path $packageRoot $directoryName) `
                -Recurse `
                -Force
        }

        $packageArchive = Join-Path $ArtifactRoot "WickedEditor-$currentConfiguration.zip"
        if (Test-Path $packageArchive -PathType Leaf) {
            Remove-Item -Path $packageArchive -Force
        }
        Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $packageArchive
        $outputRecord.package = Get-RenegadeFileRecord -Path $packageArchive
        $result.outputs += $outputRecord
    }

    $stopwatch.Stop()
    $result.status = "PASS"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

    Write-Host ""
    Write-Host "Windows build completed successfully."
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
