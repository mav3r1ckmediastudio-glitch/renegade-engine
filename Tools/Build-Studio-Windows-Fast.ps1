#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release",

    [string]$ArtifactRoot = "artifacts/studio-fast/ci-Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    throw "CMake is required for the fast Studio build."
}
$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninjaCommand) {
    throw "Ninja is required for the fast Studio build."
}
$sccacheCommand = Get-Command sccache -ErrorAction SilentlyContinue
if (-not $sccacheCommand) {
    throw "sccache is required for the fast Studio build."
}
$ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $ctestCommand) {
    throw "CTest is required for the fast Studio build."
}

$buildRoot = Join-Path $repositoryRoot "BUILD\renegade-fast"
if (Test-Path $buildRoot -PathType Container) {
    Remove-Item -Path $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

$ArtifactRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $ArtifactRoot))
if (Test-Path $ArtifactRoot -PathType Container) {
    Remove-Item -Path $ArtifactRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $ArtifactRoot -Force | Out-Null

$renegadeCommit = (& git -C $repositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()
$startedUtc = (Get-Date).ToUniversalTime()
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$resultPath = Join-Path $ArtifactRoot "build-result.json"
$result = [ordered]@{
    schemaVersion = 1
    lane = "studio-fast"
    authoritative = $false
    status = "IN_PROGRESS"
    startedUtc = $startedUtc.ToString("o")
    completedUtc = $null
    durationSeconds = $null
    renegadeCommit = $renegadeCommit
    wickedCommit = $wickedCommit
    configuration = $Configuration
    generator = "Ninja Multi-Config"
    compilerCache = "sccache"
    package = $null
    error = $null
}

function Get-FastFileRecord {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Expected file was not produced: $Path"
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $item.FullName
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

try {
    & $sccacheCommand.Source --zero-stats | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "sccache --zero-stats failed."
    }

    Invoke-RenegadeLoggedCommand `
        -FilePath $cmakeCommand.Source `
        -ArgumentList @(
            "-S", $repositoryRoot,
            "-B", $buildRoot,
            "-G", "Ninja Multi-Config",
            "-DRENEGADE_EMBED_SHADERS=ON",
            "-DCMAKE_C_COMPILER_LAUNCHER=$($sccacheCommand.Source)",
            "-DCMAKE_CXX_COMPILER_LAUNCHER=$($sccacheCommand.Source)"
        ) `
        -WorkingDirectory $repositoryRoot `
        -LogPath (Join-Path $ArtifactRoot "logs\01-configure.log")

    Invoke-RenegadeLoggedCommand `
        -FilePath $cmakeCommand.Source `
        -ArgumentList @(
            "--build", $buildRoot,
            "--config", $Configuration,
            "--parallel"
        ) `
        -WorkingDirectory $repositoryRoot `
        -LogPath (Join-Path $ArtifactRoot "logs\02-build.log")

    Invoke-RenegadeLoggedCommand `
        -FilePath $ctestCommand.Source `
        -ArgumentList @(
            "--test-dir", $buildRoot,
            "-C", $Configuration,
            "--output-on-failure"
        ) `
        -WorkingDirectory $repositoryRoot `
        -LogPath (Join-Path $ArtifactRoot "logs\03-tests.log")

    $studioDirectory = Join-Path $buildRoot "Studio\$Configuration"
    $studioExecutable = Join-Path $studioDirectory "RenegadeStudio.exe"
    $studioDxCompiler = Join-Path $studioDirectory "dxcompiler.dll"
    $runtimeDirectory = Join-Path $buildRoot "Runtime\$Configuration"
    $runtimeExecutable = Join-Path $runtimeDirectory "RenegadeRuntime.exe"
    $runtimeDxCompiler = Join-Path $runtimeDirectory "dxcompiler.dll"

    foreach ($required in @($studioExecutable, $studioDxCompiler, $runtimeExecutable, $runtimeDxCompiler)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Fast Studio package is missing required output: $required"
        }
    }

    $process = Start-Process `
        -FilePath $studioExecutable `
        -ArgumentList "startup-smoke" `
        -Wait `
        -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Fast Studio startup smoke failed with process exit code $($process.ExitCode)."
    }

    $packageRoot = Join-Path $ArtifactRoot "packages\RenegadeStudio-Fast-$Configuration"
    New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
    Copy-Item -LiteralPath $studioExecutable -Destination $packageRoot -Force
    Copy-Item -LiteralPath $studioDxCompiler -Destination $packageRoot -Force

    $studioContent = Join-Path $studioDirectory "Content"
    if (Test-Path -LiteralPath $studioContent -PathType Container) {
        Copy-Item -LiteralPath $studioContent -Destination (Join-Path $packageRoot "Content") -Recurse -Force
    }

    $studioPackageSource = Join-Path $repositoryRoot "Studio\package"
    if (Test-Path -LiteralPath $studioPackageSource -PathType Container) {
        Copy-Item -Path (Join-Path $studioPackageSource "*") -Destination $packageRoot -Recurse -Force
    }

    $runtimePackageRoot = Join-Path $packageRoot "Runtime"
    New-Item -ItemType Directory -Path $runtimePackageRoot -Force | Out-Null
    Copy-Item -LiteralPath $runtimeExecutable -Destination $runtimePackageRoot -Force
    Copy-Item -LiteralPath $runtimeDxCompiler -Destination $runtimePackageRoot -Force
    $runtimeContent = Join-Path $runtimeDirectory "Content"
    if (Test-Path -LiteralPath $runtimeContent -PathType Container) {
        Copy-Item -LiteralPath $runtimeContent -Destination (Join-Path $runtimePackageRoot "Content") -Recurse -Force
    }

    $archivePath = Join-Path $ArtifactRoot "RenegadeStudio-Fast-$Configuration.zip"
    if (Test-Path -LiteralPath $archivePath -PathType Leaf) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $archivePath
    $result.package = Get-FastFileRecord -Path $archivePath

    & $sccacheCommand.Source --show-stats | Set-Content -Path (Join-Path $ArtifactRoot "sccache-stats.txt") -Encoding UTF8
    if ($LASTEXITCODE -ne 0) {
        throw "sccache --show-stats failed."
    }

    $stopwatch.Stop()
    $result.status = "PASS"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

    Write-Host "RENEGADE_STUDIO_FAST_PASS configuration=$Configuration durationSeconds=$($result.durationSeconds)"
    Write-Host "Package: $archivePath"
}
catch {
    $stopwatch.Stop()
    $result.status = "FAIL"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result.error = $_.Exception.Message
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8
    try {
        & $sccacheCommand.Source --show-stats | Set-Content -Path (Join-Path $ArtifactRoot "sccache-stats.txt") -Encoding UTF8
    }
    catch {
    }
    throw
}
