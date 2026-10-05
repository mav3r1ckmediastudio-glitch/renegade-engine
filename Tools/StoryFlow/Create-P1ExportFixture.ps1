param(
    [Parameter(Mandatory=$true)][string]$SourceProject,
    [Parameter(Mandatory=$true)][string]$DestinationRoot
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$source=[IO.Path]::GetFullPath($SourceProject)
$root=Split-Path -Parent $source
$dest=[IO.Path]::GetFullPath($DestinationRoot)
if(Test-Path -LiteralPath $dest){throw "Destination already exists; accepted fixtures are never replaced."}
$lines=Get-Content -LiteralPath $source
$sceneLine=@($lines|Where-Object {$_ -match '^startup_scene\s*='})
if($sceneLine.Count -ne 1){throw "Expected one source startup_scene"}
$sceneRelative=($sceneLine[0] -split '=',2)[1].Trim()
$scene=[IO.Path]::GetFullPath((Join-Path $root $sceneRelative))
if(-not $scene.StartsWith($root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){
    throw "Source scene must be inside its project"
}
$inputPath=Join-Path $root 'Content\Data\GameplayInput.renegade-input'
if(-not (Test-Path $scene) -or -not (Test-Path $inputPath)){throw "Missing control scene or input map"}
$projectId=[guid]::NewGuid().ToString('D')
$flowId=[guid]::NewGuid().ToString('D')
$sceneId=[guid]::NewGuid().ToString('D')
$startId=[guid]::NewGuid().ToString('D')
$levelId=[guid]::NewGuid().ToString('D')
$completeId=[guid]::NewGuid().ToString('D')
$routeStart=[guid]::NewGuid().ToString('D')
$routeComplete=[guid]::NewGuid().ToString('D')
foreach($folder in @('Content\Scenes','Content\Flow','Content\Data')){
    New-Item -ItemType Directory -Path (Join-Path $dest $folder) -Force|Out-Null
}
$sceneCopy=Join-Path $dest 'Content\Scenes\ProxyProof.wiscene'
$inputCopy=Join-Path $dest 'Content\Data\GameplayInput.renegade-input'
Copy-Item -LiteralPath $scene -Destination $sceneCopy
Copy-Item -LiteralPath $inputPath -Destination $inputCopy
$encoding=New-Object Text.UTF8Encoding($false)
$meta=@"
format = renegade-document
version = 1
[document]
id = $sceneId
project_id = $projectId
type = scene
path_hint = Content/Scenes/ProxyProof.wiscene
generator = Renegade P1 export fixture
migrated_from = 0
"@
[IO.File]::WriteAllText("$sceneCopy.rmeta",$meta+[Environment]::NewLine,$encoding)
$flow=@"
format = renegade-document
version = 1
[document]
id = $flowId
project_id = $projectId
type = story-flow
path_hint = Content/Flow/Main.renegade-flow
generator = Renegade P1 export fixture
migrated_from = 0
[flow]
start_node = $startId
node_count = 3
route_count = 2
[node_0]
id = $startId
kind = game_start
name = Game Start
scene_asset_id =
scene_path_hint =
[node_1]
id = $levelId
kind = level
name = Proxy Proof
scene_asset_id = $sceneId
scene_path_hint = Content/Scenes/ProxyProof.wiscene
[node_2]
id = $completeId
kind = complete_game
name = Complete Game
scene_asset_id =
scene_path_hint =
[route_0]
id = $routeStart
source = $startId
outcome = renegade.flow.start
destination = $levelId
destination_entry = default
priority = 0
condition_count = 0
[route_1]
id = $routeComplete
source = $levelId
outcome = level.complete
destination = $completeId
destination_entry =
priority = 0
condition_count = 0
"@
[IO.File]::WriteAllText((Join-Path $dest 'Content\Flow\Main.renegade-flow'),$flow+[Environment]::NewLine,$encoding)
$descriptor=@"
format = renegade-project
version = 1
[project]
project_id = $projectId
name = P1 Export Proof
startup_scene = Content/Scenes/ProxyProof.wiscene
startup_flow_id = $flowId
startup_flow = Content/Flow/Main.renegade-flow
startup_screen_id =
startup_screen =
[dependencies]
always_include_format = 1
always_include_count = 1
always_include_0_class = data
always_include_0_path = Content/Data/GameplayInput.renegade-input
"@
$projectCopy=Join-Path $dest 'P1ExportProof.renegade'
[IO.File]::WriteAllText($projectCopy,$descriptor+[Environment]::NewLine,$encoding)
foreach($pair in @(@($scene,$sceneCopy),@($inputPath,$inputCopy))){
    if((Get-FileHash $pair[0]).Hash -ne (Get-FileHash $pair[1]).Hash){throw "Copied fixture bytes differ"}
}
Write-Output "EXPORT_PROJECT=$projectCopy"
Write-Output 'SCENE_INPUT_HASHES_MATCH=True'
