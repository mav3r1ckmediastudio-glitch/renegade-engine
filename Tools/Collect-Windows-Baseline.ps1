#Requires -Version 5.1

[CmdletBinding()]
param(
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

function Get-OptionalCommandOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName,
        [string[]]$ArgumentList = @()
    )

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $command) {
        return $null
    }

    $output = (& $command.Source @ArgumentList 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        return "Command failed: $output"
    }
    return $output
}

function ConvertTo-MarkdownValue {
    param($Value)

    if ($null -eq $Value) {
        return "Not detected"
    }
    if ($Value -is [System.Array]) {
        if ($Value.Count -eq 0) {
            return "None detected"
        }
        return (($Value | ForEach-Object { "$_" }) -join "<br>")
    }
    $text = "$Value"
    if ([string]::IsNullOrWhiteSpace($text)) {
        return "Not detected"
    }
    return $text.Replace("`r", "").Replace("`n", "<br>").Replace("|", "\|")
}

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputRoot = Join-Path $repositoryRoot "artifacts\phase1\system-$timestamp"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$operatingSystem = Get-CimInstance Win32_OperatingSystem
$computerSystem = Get-CimInstance Win32_ComputerSystem
$videoControllers = @(Get-CimInstance Win32_VideoController | ForEach-Object {
    [ordered]@{
        name = $_.Name
        driverVersion = $_.DriverVersion
        driverDate = if ($_.DriverDate) { $_.DriverDate.ToString("o") } else { $null }
        adapterRamBytes = $_.AdapterRAM
    }
})

$msbuildPath = $null
$msbuildVersion = $null
try {
    $msbuildPath = Get-RenegadeMSBuildPath
    $msbuildVersion = (& $msbuildPath -version -nologo 2>&1 | Out-String).Trim()
}
catch {
    $msbuildVersion = "Not detected: $($_.Exception.Message)"
}

$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
$visualStudio = @()
if ($programFilesX86) {
    $vswherePath = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswherePath -PathType Leaf) {
        $visualStudioJson = (& $vswherePath -products * -format json -utf8 | Out-String)
        if (-not [string]::IsNullOrWhiteSpace($visualStudioJson)) {
            $visualStudio = @(ConvertFrom-Json $visualStudioJson | ForEach-Object {
                [ordered]@{
                    displayName = $_.displayName
                    installationVersion = $_.installationVersion
                    productId = $_.productId
                    installationPath = $_.installationPath
                }
            })
        }
    }
}

$msvcToolsets = @()
foreach ($installation in $visualStudio) {
    $toolsetRoot = Join-Path $installation.installationPath "VC\Tools\MSVC"
    if (Test-Path $toolsetRoot -PathType Container) {
        $msvcToolsets += @(Get-ChildItem $toolsetRoot -Directory | Select-Object -ExpandProperty Name)
    }
}
$msvcToolsets = @($msvcToolsets | Sort-Object -Unique)

$windowsSdkVersions = @()
$kitsRegistryPath = "HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
$kitsRoot = $null
$kitsRecord = Get-ItemProperty $kitsRegistryPath -ErrorAction SilentlyContinue
if ($kitsRecord -and ($kitsRecord.PSObject.Properties.Name -contains "KitsRoot10")) {
    $kitsRoot = $kitsRecord.KitsRoot10
}
if ($kitsRoot) {
    $includeRoot = Join-Path $kitsRoot "Include"
    if (Test-Path $includeRoot -PathType Container) {
        $windowsSdkVersions = @(
            Get-ChildItem $includeRoot -Directory |
                Select-Object -ExpandProperty Name |
                Sort-Object -Descending
        )
    }
}

$vulkanInfo = Get-OptionalCommandOutput -CommandName "vulkaninfo" -ArgumentList @("--summary")
$renegadeCommit = (& git -C $repositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()

$record = [ordered]@{
    schemaVersion = 1
    capturedUtc = (Get-Date).ToUniversalTime().ToString("o")
    renegadeCommit = $renegadeCommit
    wickedCommit = $wickedCommit
    host = [ordered]@{
        computerManufacturer = $computerSystem.Manufacturer
        computerModel = $computerSystem.Model
        osCaption = $operatingSystem.Caption
        osVersion = $operatingSystem.Version
        osBuildNumber = $operatingSystem.BuildNumber
        architecture = $operatingSystem.OSArchitecture
        powershellVersion = $PSVersionTable.PSVersion.ToString()
    }
    tools = [ordered]@{
        git = Get-OptionalCommandOutput -CommandName "git" -ArgumentList @("--version")
        cmake = Get-OptionalCommandOutput -CommandName "cmake" -ArgumentList @("--version")
        msbuildPath = $msbuildPath
        msbuildVersion = $msbuildVersion
        visualStudio = $visualStudio
        msvcToolsets = $msvcToolsets
        windowsSdkVersions = $windowsSdkVersions
        vulkanInfo = $vulkanInfo
    }
    graphics = $videoControllers
    ci = [ordered]@{
        githubActions = [Environment]::GetEnvironmentVariable("GITHUB_ACTIONS")
        runnerName = [Environment]::GetEnvironmentVariable("RUNNER_NAME")
        runnerOs = [Environment]::GetEnvironmentVariable("RUNNER_OS")
        runnerArchitecture = [Environment]::GetEnvironmentVariable("RUNNER_ARCH")
        imageOs = [Environment]::GetEnvironmentVariable("ImageOS")
        imageVersion = [Environment]::GetEnvironmentVariable("ImageVersion")
    }
}

$jsonPath = Join-Path $OutputRoot "toolchain.json"
$record | ConvertTo-Json -Depth 10 | Set-Content -Path $jsonPath -Encoding UTF8

$visualStudioSummary = @($visualStudio | ForEach-Object {
    "$($_.displayName) $($_.installationVersion)"
})
$gpuSummary = @($videoControllers | ForEach-Object {
    "$($_.name), driver $($_.driverVersion)"
})

$markdown = @"
# Windows Toolchain Evidence

| Field | Observed value |
|---|---|
| Captured UTC | $(ConvertTo-MarkdownValue $record.capturedUtc) |
| Renegade commit | ``$renegadeCommit`` |
| Wicked commit | ``$wickedCommit`` |
| Operating system | $(ConvertTo-MarkdownValue "$($record.host.osCaption) $($record.host.osVersion), build $($record.host.osBuildNumber), $($record.host.architecture)") |
| PowerShell | $(ConvertTo-MarkdownValue $record.host.powershellVersion) |
| Visual Studio | $(ConvertTo-MarkdownValue $visualStudioSummary) |
| MSVC toolsets | $(ConvertTo-MarkdownValue $msvcToolsets) |
| MSBuild | $(ConvertTo-MarkdownValue $msbuildVersion) |
| Windows SDKs | $(ConvertTo-MarkdownValue $windowsSdkVersions) |
| CMake | $(ConvertTo-MarkdownValue $record.tools.cmake) |
| Git | $(ConvertTo-MarkdownValue $record.tools.git) |
| GPU and driver | $(ConvertTo-MarkdownValue $gpuSummary) |
| Vulkan runtime | $(ConvertTo-MarkdownValue $vulkanInfo) |
| GitHub runner image | $(ConvertTo-MarkdownValue "$($record.ci.imageOs) $($record.ci.imageVersion)") |

This file records detection only. Build results are written separately by
``Tools/Build-Windows.ps1`` and visual results by ``Tools/Run-Windows-Smoke.ps1``.
"@

$markdownPath = Join-Path $OutputRoot "toolchain.md"
$markdown | Set-Content -Path $markdownPath -Encoding UTF8

Write-Host "Windows toolchain evidence written to:"
Write-Host "  $jsonPath"
Write-Host "  $markdownPath"
