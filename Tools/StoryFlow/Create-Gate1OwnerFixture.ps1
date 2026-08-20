param(
    [Parameter(Mandatory = $true)]
    [string]$SourceProject,

    [string]$DestinationRoot = "",

    [switch]$Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Read-RenegadeProjectValue {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Lines,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $prefix = "$Key ="
    foreach ($line in $Lines) {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $trimmed.Substring($prefix.Length).Trim()
        }
    }
    return ""
}

function New-RenegadeStableId {
    return [guid]::NewGuid().ToString("D").ToLowerInvariant()
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

$sourceDescriptor = [System.IO.Path]::GetFullPath($SourceProject)
if (-not (Test-Path -LiteralPath $sourceDescriptor -PathType Leaf)) {
    throw "Source Renegade project descriptor does not exist: $sourceDescriptor"
}

$sourceLines = Get-Content -LiteralPath $sourceDescriptor
$sourceStartupScene = Read-RenegadeProjectValue -Lines $sourceLines -Key "startup_scene"
if ([string]::IsNullOrWhiteSpace($sourceStartupScene)) {
    throw "Source project does not declare startup_scene."
}

$sourceProjectRoot = Split-Path -Parent $sourceDescriptor
$sourceScenePath = Join-Path $sourceProjectRoot ($sourceStartupScene -replace '/', [System.IO.Path]::DirectorySeparatorChar)
if (-not (Test-Path -LiteralPath $sourceScenePath -PathType Leaf)) {
    throw "Source startup scene does not exist: $sourceScenePath"
}

if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $parent = Split-Path -Parent $sourceProjectRoot
    $DestinationRoot = Join-Path $parent "Renegade-StoryFlow-Gate1-Fixture"
}
$fixtureRoot = [System.IO.Path]::GetFullPath($DestinationRoot)

if ([System.StringComparer]::OrdinalIgnoreCase.Equals($fixtureRoot.TrimEnd('\', '/'), $sourceProjectRoot.TrimEnd('\', '/'))) {
    throw "Fixture destination must not be the source project directory."
}

if (Test-Path -LiteralPath $fixtureRoot) {
    if (-not $Force) {
        throw "Fixture destination already exists. Re-run with -Force to replace it: $fixtureRoot"
    }
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
}

$sceneDirectory = Join-Path $fixtureRoot "Content\Scenes"
$flowDirectory = Join-Path $fixtureRoot "Content\Flow"
New-Item -ItemType Directory -Path $sceneDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $flowDirectory -Force | Out-Null

$projectId = New-RenegadeStableId
$flowDocumentId = New-RenegadeStableId
$gameStartId = New-RenegadeStableId
$levelOneNodeId = New-RenegadeStableId
$levelTwoNodeId = New-RenegadeStableId
$completeNodeId = New-RenegadeStableId
$levelOneSceneId = New-RenegadeStableId
$levelTwoSceneId = New-RenegadeStableId
$routeStartId = New-RenegadeStableId
$routeLevelOneId = New-RenegadeStableId
$routeLevelTwoId = New-RenegadeStableId

$levelOnePath = Join-Path $sceneDirectory "LevelOne.wiscene"
$levelTwoPath = Join-Path $sceneDirectory "LevelTwo.wiscene"
Copy-Item -LiteralPath $sourceScenePath -Destination $levelOnePath
Copy-Item -LiteralPath $sourceScenePath -Destination $levelTwoPath

$levelOneMeta = @"
format = renegade-document
version = 1

[document]
id = $levelOneSceneId
project_id = $projectId
type = scene
path_hint = Content/Scenes/LevelOne.wiscene
generator = Renegade Story Flow Gate 1 owner fixture
migrated_from = 0
"@
Write-Utf8NoBom -Path "$levelOnePath.rmeta" -Content $levelOneMeta

$levelTwoMeta = @"
format = renegade-document
version = 1

[document]
id = $levelTwoSceneId
project_id = $projectId
type = scene
path_hint = Content/Scenes/LevelTwo.wiscene
generator = Renegade Story Flow Gate 1 owner fixture
migrated_from = 0
"@
Write-Utf8NoBom -Path "$levelTwoPath.rmeta" -Content $levelTwoMeta

$flowPath = Join-Path $flowDirectory "Main.renegade-flow"
$flowDocument = @"
format = renegade-document
version = 1

[document]
id = $flowDocumentId
project_id = $projectId
type = story-flow
path_hint = Content/Flow/Main.renegade-flow
generator = Renegade Story Flow Gate 1 owner fixture
migrated_from = 0

[flow]
start_node = $gameStartId
node_count = 4
route_count = 3

[node_0]
id = $gameStartId
kind = game_start
name = Game Start
scene_asset_id =
scene_path_hint =

[node_1]
id = $levelOneNodeId
kind = level
name = Level One
scene_asset_id = $levelOneSceneId
scene_path_hint = Content/Scenes/LevelOne.wiscene

[node_2]
id = $levelTwoNodeId
kind = level
name = Level Two
scene_asset_id = $levelTwoSceneId
scene_path_hint = Content/Scenes/LevelTwo.wiscene

[node_3]
id = $completeNodeId
kind = complete_game
name = Complete Game
scene_asset_id =
scene_path_hint =

[route_0]
id = $routeStartId
source = $gameStartId
outcome = renegade.flow.start
destination = $levelOneNodeId
destination_entry = default
priority = 0
condition_count = 0

[route_1]
id = $routeLevelOneId
source = $levelOneNodeId
outcome = level.complete
destination = $levelTwoNodeId
destination_entry = from-level-one
priority = 0
condition_count = 0

[route_2]
id = $routeLevelTwoId
source = $levelTwoNodeId
outcome = level.complete
destination = $completeNodeId
destination_entry =
priority = 0
condition_count = 0
"@
Write-Utf8NoBom -Path $flowPath -Content $flowDocument

$descriptorPath = Join-Path $fixtureRoot "StoryFlowGate1Fixture.renegade"
$projectDocument = @"
format = renegade-project
version = 1

[project]
project_id = $projectId
name = Story Flow Gate 1 Fixture
startup_scene = Content/Scenes/LevelOne.wiscene
startup_flow_id = $flowDocumentId
startup_flow = Content/Flow/Main.renegade-flow
"@
Write-Utf8NoBom -Path $descriptorPath -Content $projectDocument

$required = @(
    $descriptorPath,
    $flowPath,
    $levelOnePath,
    "$levelOnePath.rmeta",
    $levelTwoPath,
    "$levelTwoPath.rmeta"
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fixture creation failed; expected file is missing: $path"
    }
}

Write-Host ""
Write-Host "Renegade Story Flow Gate 1 owner fixture created successfully." -ForegroundColor Green
Write-Host ""
Write-Host "SOURCE PROJECT (unchanged): $sourceDescriptor"
Write-Host "FIXTURE PROJECT:            $descriptorPath"
Write-Host "FLOW:                       Game Start -> Level One -> Level Two -> Complete Game"
Write-Host ""
Write-Host "Open the fixture project in the Gate 1 Release build once startup_flow Studio integration has passed CI."
