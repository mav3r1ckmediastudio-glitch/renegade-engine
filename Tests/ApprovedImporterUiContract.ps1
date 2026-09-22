# Lightweight source-level guard; owner visual inspection is still mandatory.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$head = (& git -C $root rev-list -n 1 importer-ui-v4-approved-20260922).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne '5152874fa443ce011981e8f0ac50ab1da1d2cfdf') { throw 'Approved V4 tag missing or moved' }
$dashboard = Get-Content (Join-Path $root 'Studio/src/CreatorImportAnimationDashboard.h') -Raw
$studio = Get-Content (Join-Path $root 'Studio/src/StudioApplication.cpp') -Raw
$header = Get-Content (Join-Path $root 'Studio/src/StudioApplication.h') -Raw
$required = @(
    @($dashboard, 'ANIMATION SOURCES'),
    @($dashboard, 'AVAILABLE CLIPS'),
    @($dashboard, 'CLIP PREVIEW & PROPERTIES'),
    @($dashboard, 'CHARACTER ACTION ASSIGNMENTS'),
    @($dashboard, '5.  VALIDATION'),
    @($studio, 'WindowControls::RESIZE_LEFT'),
    @($studio, 'RemoveWidget(&importScalePanel_.scrollbar_horizontal)'),
    @($studio, 'importInspectorResizePending_ = true'),
    @($header, 'importInspectorWidth_ = 390.0f'),
    @($header, 'importScalePanel_.IsVisible()')
)
foreach ($check in $required) {
    if (-not $check[0].Contains($check[1])) { throw "Approved V4 source contract lost: $($check[1])" }
}
Write-Output 'APPROVED_IMPORTER_V4_SOURCE_CONTRACT_PASS (visual and functional testing still required)'