#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string[]]$Configuration = @("Debug", "Release"),

    [switch]$Clean,

    [string]$ArtifactRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Windows-Build.Common.ps1")

$repositoryRoot = Get-RenegadeRepositoryRoot
$wickedCommit = Assert-RenegadeWickedBaseline -RepositoryRoot $repositoryRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    throw "CMake 3.19 or newer is required to build Renegade Studio."
}
$ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $ctestCommand) {
    throw "CTest is required to validate the Renegade bridge."
}

$buildRoot = Join-Path $repositoryRoot "BUILD\renegade"
if ($Clean -and (Test-Path $buildRoot -PathType Container)) {
    Remove-Item -Path $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $ArtifactRoot = Join-Path $repositoryRoot "artifacts\studio\studio-$timestamp"
}
$ArtifactRoot = [System.IO.Path]::GetFullPath($ArtifactRoot)
New-Item -ItemType Directory -Path $ArtifactRoot -Force | Out-Null

$renegadeCommit = (& git -C $repositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()
$startedUtc = (Get-Date).ToUniversalTime()
$resultPath = Join-Path $ArtifactRoot "build-result.json"
$result = [ordered]@{
    schemaVersion = 1
    status = "IN_PROGRESS"
    startedUtc = $startedUtc.ToString("o")
    completedUtc = $null
    renegadeCommit = $renegadeCommit
    wickedCommit = $wickedCommit
    configurations = @($Configuration)
    platform = "x64"
    targets = @(
        "RenegadeStudio",
        "RenegadeRuntime",
        "RenegadeBridgeTests",
        "RenegadeDependencyProcessFixture",
        "RenegadeAssetRegistryProcessFixture"
    )
    embeddedShaders = $true
    outputs = @()
    error = $null
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    Invoke-RenegadeLoggedCommand `
        -FilePath $cmakeCommand.Source `
        -ArgumentList @(
            "-S", $repositoryRoot,
            "-B", $buildRoot,
            "-A", "x64",
            "-DRENEGADE_EMBED_SHADERS=ON"
        ) `
        -WorkingDirectory $repositoryRoot `
        -LogPath (Join-Path $ArtifactRoot "logs\01-configure.log")

    foreach ($currentConfiguration in $Configuration) {
        Invoke-RenegadeLoggedCommand `
            -FilePath $cmakeCommand.Source `
            -ArgumentList @(
                "--build", $buildRoot,
                "--config", $currentConfiguration,
                "--target", "RenegadeStudio", "RenegadeRuntime", "RenegadeBridgeTests",
                "RenegadeAssetRegistryProcessFixture",
                "--parallel"
            ) `
            -WorkingDirectory $repositoryRoot `
            -LogPath (Join-Path $ArtifactRoot "logs\$currentConfiguration\02-build.log")

        Invoke-RenegadeLoggedCommand `
            -FilePath $ctestCommand.Source `
            -ArgumentList @(
                "--test-dir", $buildRoot,
                "-C", $currentConfiguration,
                "--output-on-failure"
            ) `
            -WorkingDirectory $repositoryRoot `
            -LogPath (Join-Path $ArtifactRoot "logs\$currentConfiguration\03-tests.log")

        $studioDirectory = Join-Path $buildRoot "Studio\$currentConfiguration"
        $studioExecutable = Join-Path $studioDirectory "RenegadeStudio.exe"
        $dxCompiler = Join-Path $studioDirectory "dxcompiler.dll"
        $fixtureScene = Join-Path $studioDirectory "Content\cube.wiscene"
        $runtimeDirectory = Join-Path $buildRoot "Runtime\$currentConfiguration"
        $runtimeExecutable = Join-Path $runtimeDirectory "RenegadeRuntime.exe"
        $runtimeDxCompiler = Join-Path $runtimeDirectory "dxcompiler.dll"
        $runtimeFixtureScene = Join-Path $runtimeDirectory "Content\cube.wiscene"
        $dependencyProofExecutable = Join-Path `
            $buildRoot `
            "Tests\$currentConfiguration\RenegadeDependencyProcessFixture.exe"
        $assetRegistryProofExecutable = Join-Path `
            $buildRoot `
            "Tests\$currentConfiguration\RenegadeAssetRegistryProcessFixture.exe"

        $packageRoot = Join-Path $ArtifactRoot "packages\RenegadeStudio-$currentConfiguration"
        New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
        Copy-Item -Path $studioExecutable -Destination $packageRoot -Force
        Copy-Item -Path $dxCompiler -Destination $packageRoot -Force
        Copy-Item `
            -Path (Join-Path $studioDirectory "Content") `
            -Destination (Join-Path $packageRoot "Content") `
            -Recurse `
            -Force
        Copy-Item `
            -Path (Join-Path $repositoryRoot "Studio\package\*") `
            -Destination $packageRoot `
            -Force

        # LP05 Gate 8: exercise dependency extraction from the assembled
        # package twice as separate processes. Graph outputs live outside the
        # fixture, and every fixture file is hashed before and after so a
        # nominally successful extractor cannot conceal a source mutation.
        $proofToolRoot = Join-Path $packageRoot "Tools"
        $proofProjectRoot = Join-Path $packageRoot "Proof\LP05"
        New-Item -ItemType Directory -Path $proofToolRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $proofProjectRoot -Force | Out-Null
        $packagedProofExecutable = Join-Path `
            $proofToolRoot `
            "RenegadeDependencyProcessFixture.exe"
        Copy-Item `
            -Path $dependencyProofExecutable `
            -Destination $packagedProofExecutable `
            -Force
        Copy-Item -Path $dxCompiler -Destination $proofToolRoot -Force
        Copy-Item `
            -Path (Join-Path $repositoryRoot "Tests\fixtures\lp05\process-project\*") `
            -Destination $proofProjectRoot `
            -Recurse `
            -Force

        $getProofFileRecords = {
            param([string]$Root)
            $prefixLength = $Root.TrimEnd(
                [System.IO.Path]::DirectorySeparatorChar
            ).Length + 1
            @(
                Get-ChildItem -Path $Root -File -Recurse |
                    Sort-Object FullName |
                    ForEach-Object {
                        [ordered]@{
                            path = $_.FullName.Substring($prefixLength).Replace(
                                [System.IO.Path]::DirectorySeparatorChar,
                                [char]'/'
                            )
                            bytes = $_.Length
                            sha256 = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                        }
                    }
            )
        }
        $proofFilesBefore = & $getProofFileRecords -Root $proofProjectRoot
        $proofRoot = Join-Path $ArtifactRoot "proof\$currentConfiguration"
        New-Item -ItemType Directory -Path $proofRoot -Force | Out-Null
        $proofGraphFirst = Join-Path $proofRoot "dependency-graph-1.json"
        $proofGraphSecond = Join-Path $proofRoot "dependency-graph-2.json"

        & $packagedProofExecutable $proofProjectRoot $proofGraphFirst
        if ($LASTEXITCODE -ne 0) {
            throw "LP05 Gate 8 packaged dependency proof run 1 failed with exit code $LASTEXITCODE."
        }
        & $packagedProofExecutable $proofProjectRoot $proofGraphSecond
        if ($LASTEXITCODE -ne 0) {
            throw "LP05 Gate 8 packaged dependency proof run 2 failed with exit code $LASTEXITCODE."
        }

        $firstGraphRecord = Get-RenegadeFileRecord -Path $proofGraphFirst
        $secondGraphRecord = Get-RenegadeFileRecord -Path $proofGraphSecond
        if ($firstGraphRecord.sha256 -ne $secondGraphRecord.sha256 -or
            $firstGraphRecord.bytes -ne $secondGraphRecord.bytes) {
            throw "LP05 Gate 8 separate-process dependency graphs were not byte-identical."
        }
        $proofFilesAfter = & $getProofFileRecords -Root $proofProjectRoot
        $beforeJson = $proofFilesBefore | ConvertTo-Json -Depth 4 -Compress
        $afterJson = $proofFilesAfter | ConvertTo-Json -Depth 4 -Compress
        if ($beforeJson -ne $afterJson) {
            throw "LP05 Gate 8 dependency extraction modified the packaged project fixture."
        }
        Write-Host (
            "LP05_GATE8_GRAPH_SHA256={0} BYTES={1}" -f
                $firstGraphRecord.sha256,
                $firstGraphRecord.bytes
        )
        $dependencyProofRecord = [ordered]@{
            status = "PASS"
            processRuns = 2
            executable = Get-RenegadeFileRecord -Path $packagedProofExecutable
            graph = $firstGraphRecord
            projectFileCount = $proofFilesBefore.Count
            projectFiles = $proofFilesBefore
        }

        # LC01 Gate 5: prove durable asset identity and provenance from the
        # assembled package across independent process invocations and Project
        # Open. The packaged fixture remains immutable; all controlled source
        # edits and moves occur in a disposable artifact working copy.
        $lc01ToolRoot = Join-Path $packageRoot "Tools"
        $lc01FixtureRoot = Join-Path $packageRoot "Proof\LC01"
        New-Item -ItemType Directory -Path $lc01ToolRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $lc01FixtureRoot -Force | Out-Null
        $packagedAssetRegistryProof = Join-Path `
            $lc01ToolRoot `
            "RenegadeAssetRegistryProcessFixture.exe"
        Copy-Item `
            -Path $assetRegistryProofExecutable `
            -Destination $packagedAssetRegistryProof `
            -Force
        Copy-Item `
            -Path (Join-Path $repositoryRoot "Tests\fixtures\lc01\process-project\*") `
            -Destination $lc01FixtureRoot `
            -Recurse `
            -Force

        $lc01FixtureFilesBefore = & $getProofFileRecords -Root $lc01FixtureRoot
        $lc01ProofRoot = Join-Path $ArtifactRoot "proof\$currentConfiguration\lc01"
        $lc01WorkingRoot = Join-Path $lc01ProofRoot "working-project"
        New-Item -ItemType Directory -Path $lc01WorkingRoot -Force | Out-Null
        Copy-Item -Path (Join-Path $lc01FixtureRoot "*") `
            -Destination $lc01WorkingRoot -Recurse -Force

        $lc01InitEvidence = Join-Path $lc01ProofRoot "registry-init.json"
        $lc01UpdateEvidence = Join-Path $lc01ProofRoot "registry-update.json"
        $lc01MoveEvidence = Join-Path $lc01ProofRoot "registry-move-reopen.json"
        $lc01VerifyEvidence = Join-Path $lc01ProofRoot "registry-verify-reopen.json"
        $lc01LogRoot = Join-Path $ArtifactRoot "logs\$currentConfiguration\lc01"

        Invoke-RenegadeLoggedCommand `
            -FilePath $packagedAssetRegistryProof `
            -ArgumentList @("init", $lc01WorkingRoot, $lc01InitEvidence) `
            -WorkingDirectory $packageRoot `
            -LogPath (Join-Path $lc01LogRoot "01-init.log")

        $sourcePath = Join-Path $lc01WorkingRoot "Content\Source\source.asset"
        [System.IO.File]::WriteAllText(
            $sourcePath,
            "lc01 source revision two`n",
            [System.Text.UTF8Encoding]::new($false)
        )
        Invoke-RenegadeLoggedCommand `
            -FilePath $packagedAssetRegistryProof `
            -ArgumentList @("update", $lc01WorkingRoot, $lc01UpdateEvidence) `
            -WorkingDirectory $packageRoot `
            -LogPath (Join-Path $lc01LogRoot "02-update.log")

        $movedDirectory = Join-Path $lc01WorkingRoot "Content\Source\Moved"
        New-Item -ItemType Directory -Path $movedDirectory -Force | Out-Null
        Move-Item -Path $sourcePath `
            -Destination (Join-Path $movedDirectory "source.asset") -Force
        Invoke-RenegadeLoggedCommand `
            -FilePath $packagedAssetRegistryProof `
            -ArgumentList @("move-reopen", $lc01WorkingRoot, $lc01MoveEvidence) `
            -WorkingDirectory $packageRoot `
            -LogPath (Join-Path $lc01LogRoot "03-move-reopen.log")
        Invoke-RenegadeLoggedCommand `
            -FilePath $packagedAssetRegistryProof `
            -ArgumentList @("verify-reopen", $lc01WorkingRoot, $lc01VerifyEvidence) `
            -WorkingDirectory $packageRoot `
            -LogPath (Join-Path $lc01LogRoot "04-verify-reopen.log")

        $lc01MoveRecord = Get-RenegadeFileRecord -Path $lc01MoveEvidence
        $lc01VerifyRecord = Get-RenegadeFileRecord -Path $lc01VerifyEvidence
        if ($lc01MoveRecord.sha256 -ne $lc01VerifyRecord.sha256 -or
            $lc01MoveRecord.bytes -ne $lc01VerifyRecord.bytes) {
            throw "LC01 Gate 5 reopen registry evidence was not byte-identical."
        }
        $lc01FixtureFilesAfter = & $getProofFileRecords -Root $lc01FixtureRoot
        if (($lc01FixtureFilesBefore | ConvertTo-Json -Depth 4 -Compress) -ne
            ($lc01FixtureFilesAfter | ConvertTo-Json -Depth 4 -Compress)) {
            throw "LC01 Gate 5 modified the immutable packaged fixture."
        }
        Write-Host (
            "LC01_GATE5_REGISTRY_SHA256={0} BYTES={1}" -f
                $lc01VerifyRecord.sha256,
                $lc01VerifyRecord.bytes
        )
        $assetRegistryProofRecord = [ordered]@{
            status = "PASS"
            processRuns = 4
            executable = Get-RenegadeFileRecord -Path $packagedAssetRegistryProof
            finalRegistry = $lc01VerifyRecord
            packagedFixtureFileCount = $lc01FixtureFilesBefore.Count
            packagedFixtureFiles = $lc01FixtureFilesBefore
        }

        # Test Level launches the real Runtime as a child process. Keep the
        # Runtime payload isolated under the Studio package so Studio can find
        # it without changing Runtime's own working-directory/content contract.
        $studioRuntimeRoot = Join-Path $packageRoot "Runtime"
        New-Item -ItemType Directory -Path $studioRuntimeRoot -Force | Out-Null
        Copy-Item -Path $runtimeExecutable -Destination $studioRuntimeRoot -Force
        Copy-Item -Path $runtimeDxCompiler -Destination $studioRuntimeRoot -Force
        Copy-Item `
            -Path (Join-Path $runtimeDirectory "Content") `
            -Destination (Join-Path $studioRuntimeRoot "Content") `
            -Recurse `
            -Force

        $packageArchive = Join-Path $ArtifactRoot "RenegadeStudio-$currentConfiguration.zip"
        if (Test-Path $packageArchive -PathType Leaf) {
            Remove-Item -Path $packageArchive -Force
        }
        Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $packageArchive

        $runtimePackageRoot = Join-Path $ArtifactRoot "packages\RenegadeRuntime-$currentConfiguration"
        New-Item -ItemType Directory -Path $runtimePackageRoot -Force | Out-Null
        Copy-Item -Path $runtimeExecutable -Destination $runtimePackageRoot -Force
        Copy-Item -Path $runtimeDxCompiler -Destination $runtimePackageRoot -Force
        Copy-Item `
            -Path (Join-Path $runtimeDirectory "Content") `
            -Destination (Join-Path $runtimePackageRoot "Content") `
            -Recurse `
            -Force
        Copy-Item `
            -Path (Join-Path $repositoryRoot "Runtime\package\*") `
            -Destination $runtimePackageRoot `
            -Force

        $runtimePackageArchive = Join-Path $ArtifactRoot "RenegadeRuntime-$currentConfiguration.zip"
        if (Test-Path $runtimePackageArchive -PathType Leaf) {
            Remove-Item -Path $runtimePackageArchive -Force
        }
        Compress-Archive `
            -Path (Join-Path $runtimePackageRoot "*") `
            -DestinationPath $runtimePackageArchive

        $result.outputs += [ordered]@{
            configuration = $currentConfiguration
            studio = Get-RenegadeFileRecord -Path $studioExecutable
            dxcompiler = Get-RenegadeFileRecord -Path $dxCompiler
            fixtureScene = Get-RenegadeFileRecord -Path $fixtureScene
            studioPackage = Get-RenegadeFileRecord -Path $packageArchive
            runtime = Get-RenegadeFileRecord -Path $runtimeExecutable
            runtimeDxcompiler = Get-RenegadeFileRecord -Path $runtimeDxCompiler
            runtimeFixtureScene = Get-RenegadeFileRecord -Path $runtimeFixtureScene
            runtimePackage = Get-RenegadeFileRecord -Path $runtimePackageArchive
            bridgeTests = "PASS"
            dependencyProof = $dependencyProofRecord
            assetRegistryProof = $assetRegistryProofRecord
        }
    }

    $stopwatch.Stop()
    $result.status = "PASS"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8

    Write-Host ""
    Write-Host "Renegade Studio increment completed successfully."
    Write-Host "Evidence: $resultPath"
}
catch {
    $stopwatch.Stop()
    $result.status = "FAIL"
    $result.completedUtc = (Get-Date).ToUniversalTime().ToString("o")
    $result.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    $result.error = $_.Exception.Message
    $result | ConvertTo-Json -Depth 8 | Set-Content -Path $resultPath -Encoding UTF8
    throw
}
