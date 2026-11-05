$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\Tools\assistant_presence\PortOwnership.ps1')
function Require($condition,$message){if(!$condition){throw $message}}
function Must-Throw([scriptblock]$action){
 $threw=$false;try {& $action | Out-Null} catch {$threw=$true}
 Require $threw 'An unverified owner was accepted'
}
# Regression fixture: DESIGN exposes live listeners with State=0. A State
# parameter is intentionally unsupported so the original filtered query fails.
function Get-NetTCPConnection {
 [CmdletBinding()]param()
 [pscustomobject]@{State=0;LocalPort=18386;RemotePort=0;LocalAddress='127.0.0.1';OwningProcess=$PID}
 [pscustomobject]@{State=11;LocalPort=18386;RemotePort=50000;LocalAddress='127.0.0.1';OwningProcess=0}
 [pscustomobject]@{State=5;LocalPort=18386;RemotePort=50001;LocalAddress='127.0.0.1';OwningProcess=$PID}
}
$rows=@(Get-RenegadeBoundTcpEndpoint -Port 18386)
Require ($rows.Count -eq 1 -and $rows[0].OwningProcess -eq $PID) 'Unknown-state bound listener was missed'
Require (Test-RenegadePortOccupied -Port 18386) 'Occupied unknown-state port was considered free'
$exe=[Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
$owner=Get-RenegadeVerifiedPortOwner -Port 18386 -Executable $exe -RequiredArguments @([IO.Path]::GetFileName($PSCommandPath))
Require ($owner.ProcessId -eq $PID) 'Exact process ownership was not retained'
Must-Throw {Get-RenegadeVerifiedPortOwner -Port 18386 -Executable ($exe+'.wrong') -RequiredArguments @([IO.Path]::GetFileName($PSCommandPath))}
Must-Throw {Get-RenegadeVerifiedPortOwner -Port 18386 -Executable $exe -RequiredArguments @('definitely-not-an-owned-runtime-argument')}
Remove-Item Function:\Get-NetTCPConnection
# Exercise the real Windows endpoint provider and a real loopback socket.
$listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,0)
$listener.Start();$port=$listener.LocalEndpoint.Port
try {
 $actual=@(Get-RenegadeBoundTcpEndpoint -Port $port)
 Require (@($actual|Where-Object {$_.OwningProcess -eq $PID}).Count -eq 1) 'Real loopback endpoint owner was missed'
 Require (Test-RenegadePortOccupied -Port $port) 'Real loopback listener was considered free'
 $owned=Get-RenegadeVerifiedPortOwner -Port $port -Executable $exe -RequiredArguments @([IO.Path]::GetFileName($PSCommandPath))
 Require ($owned.ProcessId -eq $PID) 'Real endpoint ownership failed'
 Must-Throw {Get-RenegadeVerifiedPortOwner -Port $port -Executable ($exe+'.foreign') -RequiredArguments @([IO.Path]::GetFileName($PSCommandPath))}
 # A provider can omit the row entirely: a live socket must still block
 # startup, while missing ownership must prevent any process termination.
 function Get-NetTCPConnection {[CmdletBinding()]param()}
 try {
  Require (Test-RenegadePortOccupied -Port $port) 'TCP fallback missed an omitted live listener'
  Must-Throw {Get-RenegadeVerifiedPortOwner -Port $port -Executable $exe -RequiredArguments @([IO.Path]::GetFileName($PSCommandPath))}
 } finally {Remove-Item Function:\Get-NetTCPConnection}
} finally {$listener.Stop()}
Require (!(Test-RenegadePortOccupied -Port $port)) 'Stopped disposable port remains occupied'
'ASSISTANT_PORT_OWNERSHIP_PASS unknown-state real-socket foreign-owner wrong-arguments'
