[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$ProofRoot,
    [Parameter(Mandatory)] [string]$BasicVtolProject,
    [Parameter(Mandatory)] [string]$OriginalVanProject,
    [Parameter(Mandatory)] [string]$MerlinProject,
    [Parameter(Mandatory)] [string]$OrioleProject,
    [Parameter(Mandatory)] [string]$OutputRoot,
    [string]$ToolsRoot = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $OutputRoot) { throw "Choose a new folder for the template library." }
$vanDescriptor = Get-Item -LiteralPath $OriginalVanProject
$vanSource = $vanDescriptor.DirectoryName
if ($vanDescriptor.Name -ne 'GroundVehicle.renegade' -or
    !(Test-Path -LiteralPath (Join-Path $vanSource 'SourceAssets/WorkshopVan.glb') -PathType Leaf) -or
    (Get-ChildItem -LiteralPath $vanSource -Directory -Recurse -Filter VanTextures)) {
    throw 'Use an original Workshop Van project containing WorkshopVan.glb.'
}
$library = New-Item -ItemType Directory -Path $OutputRoot
$templates = New-Item -ItemType Directory -Path (Join-Path $library.FullName 'Templates')
$sources = [ordered]@{
    Van = $OriginalVanProject
    BasicVTOL = $BasicVtolProject
    LuaVTOL = Join-Path $ProofRoot 'ScriptedVtolCheck/FlyingVehicle.renegade'
    Kestrel = Join-Path $ProofRoot 'FixedWingCheck/KestrelRunway/KestrelRunway.renegade'
    Vesper = Join-Path $ProofRoot 'VesperCheck/VesperVtol/VesperVtol.renegade'
    Bastion = Join-Path $ProofRoot 'BastionCheck/BastionTank/BastionTank.renegade'
    Merlin = $MerlinProject
    Oriole = $OrioleProject
}
foreach ($preset in $sources.Keys) {
    $descriptor = Get-Item -LiteralPath $sources[$preset]
    $destination = New-Item -ItemType Directory -Path (Join-Path $templates.FullName $preset)
    Copy-Item -LiteralPath $descriptor.FullName -Destination $destination.FullName
    foreach ($name in 'Content','SourceAssets','VanTextures','AssetRegistry.renegade-assets') {
        $path = Join-Path $descriptor.DirectoryName $name
        if (Test-Path -LiteralPath $path) {
            Copy-Item -LiteralPath $path -Destination $destination.FullName -Recurse -Exclude '*.bak'
        }
    }
}
Copy-Item -LiteralPath (Join-Path $ToolsRoot 'NewVehicleProject.ps1') -Destination $library.FullName
$launcher = @'
@echo off
setlocal
set "RENEGADE_TEMPLATE_HOME=%~dp0"
powershell.exe -NoProfile -Command "$root=$env:RENEGADE_TEMPLATE_HOME; $source=[IO.File]::ReadAllText((Join-Path $root 'NewVehicleProject.ps1')); & ([ScriptBlock]::Create($source)) -TemplatesRoot (Join-Path $root 'Templates')"
pause
'@
[IO.File]::WriteAllText((Join-Path $library.FullName 'New Vehicle Project.cmd'), $launcher + [Environment]::NewLine)
