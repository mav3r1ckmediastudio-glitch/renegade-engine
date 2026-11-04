# Shared portable-runtime port guards. Some Windows providers report State=0
# for live listeners; endpoint ownership and a TCP probe remain authoritative.
function Get-RenegadeBoundTcpEndpoint {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$Port)
 # Enumerate without a State filter: a missing/unknown state must not hide a
 # model that still owns its bound local endpoint and GPU allocation.
 Get-NetTCPConnection -ErrorAction Stop | Where-Object {
  $_.LocalPort -eq $Port -and $_.RemotePort -eq 0 -and $_.OwningProcess -gt 0
 } | Sort-Object LocalAddress,OwningProcess -Unique
}
function Test-RenegadeTcpPort {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$Port)
 $client=New-Object Net.Sockets.TcpClient
 try {
  $pending=$client.BeginConnect('127.0.0.1',$Port,$null,$null)
  if(!$pending.AsyncWaitHandle.WaitOne(800)){return $false}
  try {$client.EndConnect($pending);return $true} catch {return $false}
 } finally {$client.Dispose()}
}
function Test-RenegadePortOccupied {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$Port)
 if(@(Get-RenegadeBoundTcpEndpoint -Port $Port).Count){return $true}
 return Test-RenegadeTcpPort -Port $Port
}
function Get-RenegadeVerifiedPortOwner {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][int]$Port,
       [Parameter(Mandatory=$true)][string]$Executable,
       [Parameter(Mandatory=$true)][string[]]$RequiredArguments)
 $endpoints=@(Get-RenegadeBoundTcpEndpoint -Port $Port)
 if(!$endpoints.Count){
  if(Test-RenegadeTcpPort -Port $Port){throw 'Live local port ownership could not be verified'}
  return $null
 }
 if($endpoints.Count -ne 1 -or $endpoints[0].LocalAddress -ne '127.0.0.1'){
  throw 'Unexpected local runtime endpoints; nothing was stopped'
 }
 $owned=Get-CimInstance Win32_Process -Filter ('ProcessId='+$endpoints[0].OwningProcess) -ErrorAction Stop
 if(!$owned -or ![string]::Equals($owned.ExecutablePath,$Executable,[StringComparison]::OrdinalIgnoreCase)){
  throw 'Local runtime port belongs to another executable; nothing was stopped'
 }
 foreach($argument in $RequiredArguments){
  if(!$owned.CommandLine -or $owned.CommandLine.IndexOf($argument,[StringComparison]::OrdinalIgnoreCase) -lt 0){
   throw 'Local runtime arguments do not match this package; nothing was stopped'
  }
 }
 return $owned
}
