param([Parameter(Mandatory=$true)][string]$ConfigPath,[switch]$BackendOnly)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=(Resolve-Path (Join-Path $here '..\..')).Path
$leasePath=Join-Path $repo 'BUILD\assistant-gpu-session.json'
if(Test-Path $leasePath){
 $session=Get-Content $leasePath -Raw|ConvertFrom-Json
 if(@($session.leases).Count){throw 'Assistant GPU model is paused for Test Game'}
}
$config=Get-Content $ConfigPath -Raw | ConvertFrom-Json
foreach($file in @($config.server,$config.model,$config.lora,$config.manifest)){if(-not (Test-Path $file)){throw ('Missing local runtime dependency: '+$file)}}
$ports=@(8384,18386);if($BackendOnly){$ports=@(18386)}
foreach($port in $ports){if(Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue){throw ('Local runtime port already occupied: '+$port)}}
$node=Join-Path $repo 'AssistantRuntime\node\node.exe'
if(-not(Test-Path $node)){$node=(Get-Command node.exe -ErrorAction Stop).Source}
$directory=Split-Path $config.server -Parent
Push-Location $directory
try{$relativeLora=Resolve-Path $config.lora -Relative}finally{Pop-Location}
$children=@();$ok=$false
try{
 $backend=Start-Process -FilePath $config.server -ArgumentList @('-m',('"'+$config.model+'"'),'--lora-scaled',('"'+$relativeLora+':0.65"'),'-ngl','99','-c','4096','--parallel','1','--flash-attn','on','--host','127.0.0.1','--port','18386','--jinja') -WorkingDirectory $directory -WindowStyle Hidden -RedirectStandardOutput "$repo\BUILD\character-llama-out.log" -RedirectStandardError "$repo\BUILD\character-llama-err.log" -PassThru
 $children+=$backend
 $ready=$false
 for($i=0;$i -lt 120;$i++){
  try{$h=Invoke-RestMethod http://127.0.0.1:18386/health -TimeoutSec 1;if($h.status -eq 'ok'){$ready=$true;break}}catch{}
  if($backend.HasExited){break}
  Start-Sleep -Milliseconds 350
 }
 if(-not $ready){throw 'Local CUDA runtime did not become healthy'}
 if($BackendOnly){$ok=$true;Write-Output 'CHARACTER_BACKEND_READY';return}
 $gateway=Join-Path $here 'chat\llama-character-service.cjs'
 $front=Start-Process -FilePath $node -ArgumentList @(('"'+$gateway+'"'),('"'+$ConfigPath+'"')) -WorkingDirectory $repo -WindowStyle Hidden -RedirectStandardOutput "$repo\BUILD\character-gateway-out.log" -RedirectStandardError "$repo\BUILD\character-gateway-err.log" -PassThru
 $children+=$front
 for($i=0;$i -lt 40;$i++){
  try{$h=Invoke-RestMethod http://127.0.0.1:8384/health -TimeoutSec 1;if($h.runtime -eq 'llama.cpp' -and $h.streaming){$ok=$true;break}}catch{}
  if($front.HasExited){break}
  Start-Sleep -Milliseconds 250
 }
 if(-not $ok){throw 'Character gateway did not become healthy'}
 Write-Output 'CHARACTER_LLAMA_READY'
}finally{
 if(-not $ok){foreach($child in $children){if(-not $child.HasExited){Stop-Process -Id $child.Id -ErrorAction SilentlyContinue}}}
}
