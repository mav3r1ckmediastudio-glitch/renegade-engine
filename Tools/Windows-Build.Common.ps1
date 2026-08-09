#Requires -Version 5.1

Set-StrictMode -Version Latest

$script:RenegadePinnedWickedCommit = "3a800b7134aafe58461093c8abb2e274d4e64033"

function Get-RenegadeRepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Assert-RenegadeWickedBaseline {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $wickedRoot = Join-Path $RepositoryRoot "WickedEngine"
    $solutionPath = Join-Path $wickedRoot "WickedEngine.sln"

    if (-not (Test-Path $solutionPath -PathType Leaf)) {
        throw @"
Wicked Engine is not initialized at '$wickedRoot'.
Run: git submodule update --init --recursive
"@
    }

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "Git is required to verify the pinned Wicked Engine baseline."
    }

    $actualCommitOutput = @(& git -C $wickedRoot rev-parse HEAD 2>$null)
    $gitSucceeded = $?
    $actualCommit = $actualCommitOutput | Select-Object -First 1
    if (-not $gitSucceeded -or [string]::IsNullOrWhiteSpace($actualCommit)) {
        throw "Could not read the Wicked Engine submodule commit."
    }

    $actualCommit = $actualCommit.Trim()
    if ($actualCommit -ne $script:RenegadePinnedWickedCommit) {
        throw @"
Wicked Engine is at '$actualCommit', but Renegade requires
'$script:RenegadePinnedWickedCommit'. Run:
git submodule update --init --recursive
"@
    }

    $trackedChanges = @(& git -C $wickedRoot status --porcelain --untracked-files=no)
    $gitSucceeded = $?
    if (-not $gitSucceeded) {
        throw "Could not verify the Wicked Engine working tree."
    }
    if ($trackedChanges) {
        throw "The Wicked Engine submodule has tracked changes. Preserve or revert them before building."
    }

    return $actualCommit
}

function Get-RenegadeMSBuildPath {
    $configuredMSBuild = [Environment]::GetEnvironmentVariable("MSBUILD_EXE_PATH")
    if ($configuredMSBuild -and (Test-Path $configuredMSBuild -PathType Leaf)) {
        return $configuredMSBuild
    }

    $msbuildCommand = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if (-not $msbuildCommand) {
        $msbuildCommand = Get-Command msbuild -ErrorAction SilentlyContinue
    }
    if ($msbuildCommand) {
        return $msbuildCommand.Source
    }

    $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    if ($programFilesX86) {
        $vswherePath = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswherePath -PathType Leaf) {
            $candidate = (& $vswherePath `
                -latest `
                -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -find "MSBuild\**\Bin\MSBuild.exe" |
                Select-Object -First 1)
            if ($candidate -and (Test-Path $candidate -PathType Leaf)) {
                return $candidate
            }
        }
    }

    throw @"
MSBuild was not found. Install Visual Studio with 'Desktop development with C++',
the v145 MSVC toolset, and a Windows 10/11 SDK, or run this script from a
Visual Studio Developer PowerShell.
"@
}

function Invoke-RenegadeLoggedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    $logDirectory = Split-Path -Parent $LogPath
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null

    @(
        "Started: $((Get-Date).ToUniversalTime().ToString('o'))"
        "Working directory: $WorkingDirectory"
        "Command: $FilePath $($ArgumentList -join ' ')"
        ""
    ) | Set-Content -Path $LogPath -Encoding UTF8

    $exitCode = -1
    $commandOutputPath = "$LogPath.command-output"
    if (Test-Path $commandOutputPath -PathType Leaf) {
        Remove-Item -Path $commandOutputPath -Force
    }
    Push-Location $WorkingDirectory
    try {
        # Do not pipe the native process directly into Tee-Object. PowerShell
        # can leave $LASTEXITCODE unchanged when a native command is the first
        # element of a pipeline, which previously allowed a failing CTest run
        # to be recorded as PASS. Capture first, record the exit code
        # immediately, then replay the output to the console and durable log.
        $LASTEXITCODE = 0
        & $FilePath @ArgumentList *> $commandOutputPath
        $exitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location

        if (Test-Path $commandOutputPath -PathType Leaf) {
            Get-Content -Path $commandOutputPath |
                Tee-Object -FilePath $LogPath -Append |
                ForEach-Object { Write-Host $_ }
            Remove-Item -Path $commandOutputPath -Force
        }
    }

    if ($exitCode -ne 0) {
        throw "Command failed with exit code $exitCode. See '$LogPath'."
    }
}

function Get-RenegadeFileRecord {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path -PathType Leaf)) {
        throw "Expected build output is missing: '$Path'."
    }

    $item = Get-Item $Path
    $hash = Get-FileHash -Path $Path -Algorithm SHA256
    return [ordered]@{
        path = $item.FullName
        bytes = $item.Length
        sha256 = $hash.Hash.ToLowerInvariant()
    }
}
