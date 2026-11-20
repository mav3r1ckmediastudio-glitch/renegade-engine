#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Archive,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ArchiveSha256,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{40}$')][string]$Revision,
    [Parameter(Mandatory)][string]$ExpectedFlowNode,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $ArchiveSha256) {
    throw 'Standalone ZIP digest mismatch.'
}
if (Test-Path -LiteralPath $Output) { throw 'Choose a fresh output directory.' }
$root = (New-Item -ItemType Directory -Path $Output).FullName
$game = Join-Path $root 'game'
$launch = (New-Item -ItemType Directory -Path (Join-Path $root 'unrelated')).FullName
$profile = (New-Item -ItemType Directory -Path (Join-Path $root 'profile')).FullName
Expand-Archive -LiteralPath $archivePath -DestinationPath $game
$manifestPath = Join-Path $game 'package-manifest.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$report = Get-Content -Raw -LiteralPath (Join-Path $game 'build-report.json') | ConvertFrom-Json
if ($report.renegade_revision -ne $Revision -or $manifest.configuration -ne 'Release' -or
    $manifest.platform -ne 'windows-x64' -or $report.smoke_test -ne 'passed_gate4') {
    throw 'The package does not match the requested Release revision and export smoke.'
}
$exeName = [string]$manifest.executable
if ($exeName -ne [IO.Path]::GetFileName($exeName) -or [IO.Path]::GetExtension($exeName) -ne '.exe') {
    throw 'Expected a named game executable in the installation root.'
}
$gamePrefix = [IO.Path]::GetFullPath($game) + [IO.Path]::DirectorySeparatorChar
foreach ($entry in $manifest.files) {
    $file = [IO.Path]::GetFullPath((Join-Path $game $entry.path))
    if (!$file.StartsWith($gamePrefix, [StringComparison]::OrdinalIgnoreCase) -or
        (Get-Item -LiteralPath $file).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Invalid manifest entry $($entry.path)"
    }
}
$frames = Join-Path $root 'frames.csv'
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = Join-Path $game $exeName
$start.Arguments = 'alwaysactive'
$start.WorkingDirectory = $launch
$start.UseShellExecute = $false
$start.EnvironmentVariables['LOCALAPPDATA'] = $profile
$start.EnvironmentVariables['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
$start.EnvironmentVariables['RENEGADE_G13E13_PERF_CSV'] = $frames
$start.EnvironmentVariables['RENEGADE_G13E13_FRAMES'] = '60'
$start.EnvironmentVariables['RENEGADE_G13E13_NO_SCREENSHOT'] = '1'
$proc = [Diagnostics.Process]::Start($start)
try {
    if (!$proc.WaitForExit(300000)) {
        $proc.Kill()
        throw 'The named game timed out before its measured run finished.'
    }
    if ($proc.ExitCode -ne 0) { throw "The named game exited with $($proc.ExitCode)." }
} finally { $proc.Dispose() }
$logs = @(Get-ChildItem -LiteralPath $profile -Filter RuntimeBootstrap.log -File -Recurse)
if ($logs.Count -ne 1) { throw 'Expected one fresh runtime bootstrap log.' }
$lines = Get-Content -LiteralPath $logs[0].FullName
$manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
foreach ($line in 'status=PASS','package_relative_launch=true','package_integrity=PASS','graphics_backend=DX12',"project_id=$($manifest.project_id)","flow_node_name=$ExpectedFlowNode","package_manifest_sha256=$manifestHash") {
    if ($line -notin $lines) { throw "Runtime did not report $line" }
}
if (@(Import-Csv -LiteralPath $frames).Count -ne 60) { throw 'Expected 60 measured runtime frames.' }
[ordered]@{
    revision = $Revision; executable = $exeName; archive_sha256 = $ArchiveSha256
    manifest_sha256 = $manifestHash; verified_files = @($manifest.files).Count
    frames = 60; exit_code = 0; flow_node = ($lines -like 'flow_node_name=*')
    host = [Environment]::OSVersion.VersionString
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'result.json') -Encoding UTF8
Get-Content -LiteralPath (Join-Path $root 'result.json')
