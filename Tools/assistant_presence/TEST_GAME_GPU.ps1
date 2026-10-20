param(
 [Parameter(Mandatory=$true)][ValidateSet('Suspend','Release','Watch')][string]$Action,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9-]{1,100}$')][string]$Lease,
 [int]$RuntimePid=0,[long]$RuntimeCreated=0
)
# Private state only; never modifies a project, voice worker or orb.
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$statePath=Join-Path $repo 'BUILD\assistant-gpu-session.json'
$configPath=Join-Path $repo 'BUILD\character-preview.json'
$mutex=New-Object Threading.Mutex($false,'Local\RenegadeAssistantGpuLease')
function Save-State($state) {
 $temporary=$statePath+'.tmp'
 $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporary -Encoding UTF8
 Move-Item -LiteralPath $temporary -Destination $statePath -Force
}
if($Action -eq 'Watch'){
 # Open the exact Runtime process before waiting; PID reuse must not release
 # a lease while a different process happens to have that PID.
 try {
  $runtime=[Diagnostics.Process]::GetProcessById($RuntimePid)
  $handle=$runtime.Handle
  if($runtime.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $RuntimeCreated){throw 'Runtime identity changed'}
  $runtime.WaitForExit()
 } catch {
  if(Get-Process -Id $RuntimePid -ErrorAction SilentlyContinue){throw}
 }
 $Action='Release'
}
$locked=$false
try {
 try {$locked=$mutex.WaitOne(15000)} catch [Threading.AbandonedMutexException] {$locked=$true}
 if(!$locked){throw 'Assistant GPU lifecycle is busy'}
 $state=$null
 if(Test-Path $statePath){$state=Get-Content $statePath -Raw|ConvertFrom-Json}
 if($Action -eq 'Suspend'){
  if(!$state){
   $state=[pscustomobject]@{leases=@();restore=$false;config=$configPath;error=$null}
  }
  if(@($state.leases) -contains $Lease){exit 0}
  $listeners=@(Get-NetTCPConnection -State Listen -LocalPort 18386 -ErrorAction SilentlyContinue)
  if($listeners.Count -gt 1){throw 'Unexpected model listeners'}
  $owned=$null
  if($listeners.Count){
   if(!(Test-Path $configPath)){throw 'Cannot verify assistant model ownership'}
   $config=Get-Content $configPath -Raw|ConvertFrom-Json
   $owned=Get-CimInstance Win32_Process -Filter ('ProcessId='+$listeners[0].OwningProcess)
   if($owned.ExecutablePath -ne $config.server -or
      $owned.CommandLine -notlike ('*'+$config.model+'*')){
    throw 'Model port is not owned by the configured assistant; not terminating it'
   }
  }
  $state.leases=@($state.leases)+$Lease
  if($owned){$state.restore=$true}
  Save-State $state  # Guard all normal model starts BEFORE unloading.
  if($owned){
   $process=[Diagnostics.Process]::GetProcessById($owned.ProcessId)
   $handle=$process.Handle
   if([math]::Abs($process.StartTime.ToUniversalTime().ToFileTimeUtc() -
      ([datetime]$owned.CreationDate).ToUniversalTime().ToFileTimeUtc()) -gt 10){throw 'Model process identity changed'}
   $process.Kill()
   if(!$process.WaitForExit(10000)){throw 'Assistant model did not unload'}
  }
  if(Get-NetTCPConnection -State Listen -LocalPort 18386 -ErrorAction SilentlyContinue){
   throw 'Assistant model is still listening; Test Game must not start'
  }
  'ASSISTANT_GPU_RELEASED'
 } elseif($state) {
  if(@($state.leases) -notcontains $Lease){exit 0}
  $state.leases=@($state.leases|Where-Object {$_ -ne $Lease})
  Save-State $state
  if(!$state.leases.Count){
   if($state.restore){
    try {
     & (Join-Path $PSScriptRoot 'START_CHARACTER_LLAMA.ps1') -ConfigPath $state.config -BackendOnly
     if(!$?){throw 'Model restore failed'}
    } catch {
     $state.error=$_.Exception.Message;Save-State $state
     throw
    }
   }
   Remove-Item -LiteralPath $statePath -Force
   'ASSISTANT_GPU_SESSION_FINISHED'
  }
 }
} finally {
 if($locked){$mutex.ReleaseMutex()}
 $mutex.Dispose()
}
