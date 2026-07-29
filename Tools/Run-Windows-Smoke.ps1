#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet("dx12", "vulkan")]
    [string[]]$GraphicsApi = @("dx12", "vulkan"),

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$ScenePath,

    [string]$EvidenceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedRoot = Join-Path $repositoryRoot "WickedEngine"
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot
$renegadeCommit = (& git -C $repositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()

$editorOutput = Join-Path $wickedRoot "BUILD\x64\$Configuration\Editor_Windows\Editor_Windows.exe"
if (-not (Test-Path $editorOutput -PathType Leaf)) {
    throw @"
The $Configuration editor has not been built. Run:
.\Tools\Build-Windows.ps1 -Configuration $Configuration
"@
}

if ($ScenePath) {
    $ScenePath = (Resolve-Path $ScenePath).Path
}

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceRoot = Join-Path $repositoryRoot "artifacts\phase1\smoke-$timestamp"
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null

$results = @()

foreach ($api in $GraphicsApi) {
    $runtimeRoot = Join-Path $EvidenceRoot "runtime-$api"
    New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null

    Copy-Item -Path $editorOutput -Destination $runtimeRoot -Force
    $builtDxCompiler = Join-Path (Split-Path -Parent $editorOutput) "dxcompiler.dll"
    $sourceDxCompiler = Join-Path $wickedRoot "WickedEngine\dxcompiler.dll"
    if (Test-Path $builtDxCompiler -PathType Leaf) {
        Copy-Item -Path $builtDxCompiler -Destination $runtimeRoot -Force
    }
    elseif (Test-Path $sourceDxCompiler -PathType Leaf) {
        Copy-Item -Path $sourceDxCompiler -Destination $runtimeRoot -Force
    }

    foreach ($fileName in @("config.ini", "startup.lua")) {
        Copy-Item -Path (Join-Path $wickedRoot "Editor\$fileName") -Destination $runtimeRoot -Force
    }
    foreach ($directoryName in @("fonts", "languages")) {
        Copy-Item `
            -Path (Join-Path $wickedRoot "Editor\$directoryName") `
            -Destination (Join-Path $runtimeRoot $directoryName) `
            -Recurse `
            -Force
    }

    $arguments = @($api)
    if ($ScenePath) {
        $arguments += $ScenePath
    }

    Write-Host ""
    Write-Host "Launching the pinned Wicked Editor with '$api'."
    Write-Host "Check that the editor opens, renders a viewport, accepts input, and exits cleanly."
    Write-Host "Close the editor when the check is complete."

    $startedUtc = (Get-Date).ToUniversalTime()
    $process = Start-Process `
        -FilePath (Join-Path $runtimeRoot "Editor_Windows.exe") `
        -ArgumentList $arguments `
        -WorkingDirectory $runtimeRoot `
        -PassThru
    $process.WaitForExit()
    $completedUtc = (Get-Date).ToUniversalTime()

    $verdict = ""
    while ($verdict -notin @("PASS", "FAIL", "SKIP")) {
        $answer = (Read-Host "Observed result for $api (P=PASS, F=FAIL, S=SKIP)").Trim().ToUpperInvariant()
        switch ($answer) {
            "P" { $verdict = "PASS" }
            "PASS" { $verdict = "PASS" }
            "F" { $verdict = "FAIL" }
            "FAIL" { $verdict = "FAIL" }
            "S" { $verdict = "SKIP" }
            "SKIP" { $verdict = "SKIP" }
        }
    }
    $notes = Read-Host "Notes, visible warnings, or screenshot filenames (optional)"

    if ($process.ExitCode -ne 0 -and $verdict -eq "PASS") {
        $verdict = "FAIL"
        $notes = "Editor exit code $($process.ExitCode). $notes".Trim()
    }

    $results += [ordered]@{
        graphicsApi = $api
        verdict = $verdict
        exitCode = $process.ExitCode
        startedUtc = $startedUtc.ToString("o")
        completedUtc = $completedUtc.ToString("o")
        scenePath = $ScenePath
        notes = $notes
        runtimeDirectory = $runtimeRoot
    }
}

$overall = if ($results.verdict -contains "FAIL") {
    "FAIL"
}
elseif ($results.verdict -contains "SKIP") {
    "PASS_WITH_LIMITATIONS"
}
else {
    "PASS"
}

$record = [ordered]@{
    schemaVersion = 1
    status = $overall
    renegadeCommit = $renegadeCommit
    wickedCommit = $wickedCommit
    configuration = $Configuration
    results = $results
}

$jsonPath = Join-Path $EvidenceRoot "smoke-result.json"
$record | ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding UTF8

$tableRows = @($results | ForEach-Object {
    "| $($_.graphicsApi) | $($_.verdict) | $($_.exitCode) | $($_.notes -replace '\|', '\|') |"
})
$markdown = @"
# Windows Graphics Smoke Result

**Overall:** $overall

**Renegade commit:** ``$renegadeCommit``

**Wicked commit:** ``$wickedCommit``

| API | Result | Exit code | Notes |
|---|---|---:|---|
$($tableRows -join "`r`n")

Screenshots are manual evidence. Place them beside this file and name them in
the notes above before requesting independent verification.
"@

$markdownPath = Join-Path $EvidenceRoot "smoke-result.md"
$markdown | Set-Content -Path $markdownPath -Encoding UTF8

Write-Host ""
Write-Host "Smoke result: $overall"
Write-Host "Evidence: $markdownPath"

if ($overall -eq "FAIL") {
    exit 1
}
