# Portable alpha launcher. All dependencies resolve inside the extracted package.
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=(Resolve-Path (Join-Path $here '..\..')).Path
$payload=Join-Path $repo 'AssistantRuntime'
$python=Join-Path $payload 'python-voice\python.exe'
$electron=Join-Path $payload 'electron\electron.exe'
$bridge=Join-Path $repo 'Tools\assistant_knowledge\bridge_server.py'
New-Item -ItemType Directory -Path (Join-Path $repo 'BUILD') -Force | Out-Null
function Healthy($port){
 try {
  $health=Invoke-RestMethod "http://127.0.0.1:$port/health" -TimeoutSec 2
  if($port -eq 18384 -and $health.ok -eq $true -and $health.read_only -eq $true){return $health}
  if($port -eq 8384 -and $health.runtime -eq 'llama.cpp' -and $health.adapter -eq 'character-v6' -and $health.streaming -eq $true){return $health}
 }catch{}
 return $null
}
foreach($required in @($python,$electron,$bridge,(Join-Path $payload 'speech\kokoro-v1.0.onnx'),(Join-Path $repo 'BUILD\assistant_knowledge.sqlite'))){if(-not(Test-Path -LiteralPath $required)){throw "Incomplete assistant package: $required"}}
$mains=@(Get-CimInstance Win32_Process -Filter "Name='electron.exe'" -ErrorAction SilentlyContinue | Where-Object {$_.CommandLine -notmatch '--type='})
if(@($mains|Where-Object {$_.CommandLine -like ('*'+$here+'*')}).Count){Write-Host 'Living Core is already running. Restore it from its tray icon.';exit 0}
if(@($mains|Where-Object {$_.CommandLine -match 'assistant_presence|Renegade_Qwen|Renegade_Daniel'}).Count){throw 'Another Renegade companion is running. Close it through its tray Quit command first; nothing was stopped.'}
if(-not(Healthy 18384)){
 if(Get-NetTCPConnection -LocalPort 18384 -State Listen -ErrorAction SilentlyContinue){throw 'Knowledge service port is occupied by another service.'}
 $proc=Start-Process $python -ArgumentList @('-u',('"'+$bridge+'"'),'--repo',('"'+(Join-Path $repo 'KnowledgeSource')+'"'),'--db',('"'+(Join-Path $repo 'BUILD\assistant_knowledge.sqlite')+'"')) -WorkingDirectory $repo -WindowStyle Hidden -RedirectStandardOutput "$repo\BUILD\knowledge-out.log" -RedirectStandardError "$repo\BUILD\knowledge-err.log" -PassThru
 for($i=0;$i -lt 60;$i++){if(Healthy 18384){break};if($proc.HasExited){throw 'Knowledge service failed to start.'};Start-Sleep -Milliseconds 250}
 if(-not(Healthy 18384)){throw 'Knowledge service did not become ready.'}
}
$env:PATH=(Join-Path $payload 'node')+';'+$env:PATH
$healthy=Healthy 8384
if(-not $healthy){
 if(Get-NetTCPConnection -LocalPort 8384 -State Listen -ErrorAction SilentlyContinue){throw 'Conversation port is occupied by another service.'}
 $config=Join-Path $repo 'BUILD\character-preview.json'
 $settings=@{enabled=$true;runtime='llama.cpp';label='character-v6';quantisation='Q5_K_M';server=(Join-Path $payload 'llama\llama-server.exe');model=(Join-Path $payload 'models\qwen4b-q5_k_m.gguf');lora=(Join-Path $payload 'models\character-v6.gguf');manifest=(Join-Path $payload 'models\model-manifest.json')}
 [System.IO.File]::WriteAllText($config,($settings|ConvertTo-Json),(New-Object System.Text.UTF8Encoding($false)))
 & (Join-Path $here 'START_CHARACTER_LLAMA.ps1') -ConfigPath $config
 if($LASTEXITCODE -and $LASTEXITCODE -ne 0){throw 'Conversation backend could not start.'}
}
$env:RENEGADE_CHARACTER_MODE='learned'
$env:RENEGADE_DANIEL_PYTHON=$python
$env:RENEGADE_DANIEL_MODEL_DIR=Join-Path $payload 'speech'
$env:RENEGADE_DANIEL_SPEED='1.10'
$env:RENEGADE_STT_PYTHON=Join-Path $payload 'python-stt\python.exe'
$env:RENEGADE_STT_MODEL_DIR=Join-Path $payload 'stt-model'
$env:RENEGADE_PRESENCE_INTEGRATED='1'
Start-Process $electron -ArgumentList @(('"'+$here+'"')) -WorkingDirectory $here
Write-Host 'Living Core launched. Use its tray Quit command to close it.'
