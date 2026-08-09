#Requires -Version 5.1

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$evidenceRoot = Join-Path $repositoryRoot "artifacts\command-runner"
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null

$successLog = Join-Path $evidenceRoot "success.log"
Invoke-RenegadeLoggedCommand `
    -FilePath $Env:ComSpec `
    -ArgumentList @("/d", "/c", "echo command-runner-success") `
    -WorkingDirectory $repositoryRoot `
    -LogPath $successLog

$failureLog = Join-Path $evidenceRoot "failure.log"
$failureObserved = $false
try {
    Invoke-RenegadeLoggedCommand `
        -FilePath $Env:ComSpec `
        -ArgumentList @("/d", "/c", "echo command-runner-failure&&exit 23") `
        -WorkingDirectory $repositoryRoot `
        -LogPath $failureLog
}
catch {
    if ($_.Exception.Message -notmatch "exit code 23") {
        throw "Command-runner probe threw the wrong failure: $($_.Exception.Message)"
    }
    $failureObserved = $true
}

if (-not $failureObserved) {
    throw "Command runner silently accepted a native process exit code of 23."
}

if (-not (Select-String -Path $successLog -SimpleMatch "command-runner-success") -or
    -not (Select-String -Path $failureLog -SimpleMatch "command-runner-failure")) {
    throw "Command runner did not preserve native process output in its logs."
}

Write-Host "PASS: native command output and non-zero exit propagation"
exit 0
