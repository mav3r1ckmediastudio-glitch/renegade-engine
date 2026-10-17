# Compile every Wicked Engine PS variant that includes the Renegade sky overlay.
# Runs only in this feature worktree. Uses pinned dxcompiler.dll (supports select()).
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$shaderRoot = Join-Path $root 'WickedEngine/WickedEngine/shaders'
$compilerDll = Join-Path $root 'WickedEngine/WickedEngine/dxcompiler.dll'
$exe = Get-ChildItem 'C:/Program Files (x86)/Windows Kits/10/bin' -Filter 'dxc.exe' -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -match '\\x64$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $exe -or -not (Test-Path $compilerDll)) {
    throw 'Windows SDK x64 dxc.exe or pinned dxcompiler.dll was not found'
}
$out = Join-Path $root 'BUILD/night-sky-check/shader-check'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $out 'dxc.exe') -Force
Copy-Item -LiteralPath $compilerDll -Destination (Join-Path $out 'dxcompiler.dll') -Force
$dxc = Join-Path $out 'dxc.exe'
$names = @('skyPS_dynamic', 'skyPS_static',
           'envMap_skyPS_dynamic', 'envMap_skyPS_static', 'sunPS')
foreach ($name in $names) {
    $outputFile = Join-Path $out "$name.dxil"
    $errorFile = Join-Path $out "$name.dx12.log"
    $sourceFile = Join-Path $shaderRoot "$name.hlsl"
    $args = "-nologo -T ps_6_0 -E main -I `"$shaderRoot`" -Fo `"$outputFile`" `"$sourceFile`""
    $process = Start-Process -FilePath $dxc -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardError $errorFile
    if ($process.ExitCode -ne 0 -or -not (Test-Path $outputFile)) {
        Get-Content $errorFile -Tail 20
        throw "Sky shader compilation failed: $name (exit $($process.ExitCode))"
    }
    Write-Output "$name : DX12 PASS ($((Get-Item $outputFile).Length) bytes)"
}
Write-Output 'Five real sky shader DX12 variants compiled successfully.'
