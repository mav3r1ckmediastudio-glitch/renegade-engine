[CmdletBinding()]
param(
    [string]$Preset,
    [string]$Name,
    [string]$Destination,
    [string]$TemplatesRoot = (Join-Path $PSScriptRoot 'Templates')
)

$ErrorActionPreference = 'Stop'
$catalogue = [ordered]@{
    Van = 'GroundVehicle.renegade'
    BasicVTOL = 'FlyingVehicle.renegade'
    LuaVTOL = 'FlyingVehicle.renegade'
    Kestrel = 'KestrelRunway.renegade'
    Vesper = 'VesperVtol.renegade'
    Bastion = 'BastionTank.renegade'
    Merlin = 'MerlinHelipad.renegade'
    Oriole = 'OrioleDock.renegade'
}

if (!$Preset) {
    $keys = @($catalogue.Keys)
    for ($i = 0; $i -lt $keys.Count; $i++) {
        Write-Host ('{0}. {1}' -f ($i + 1), $keys[$i])
    }
    $choice = 0
    if (![int]::TryParse((Read-Host 'Choose a vehicle template'), [ref]$choice) -or
        $choice -lt 1 -or $choice -gt $keys.Count) {
        throw 'Choose a number from the template list.'
    }
    $Preset = $keys[$choice - 1]
}
if (!$catalogue.Contains($Preset)) { throw "Unknown vehicle template $Preset." }
if (!$Name) { $Name = (Read-Host 'New project name').Trim() }
if (!$Destination) {
    $default = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Renegade Projects'
    $Destination = (Read-Host "Folder for the new project [$default]").Trim()
    if (!$Destination) { $Destination = $default }
}

$Name = $Name.Trim()
if ($Name.Length -eq 0 -or $Name.Length -gt 64 -or
    $Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
    $Name.EndsWith('.') -or $Name -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') {
    throw 'Enter a valid Windows project folder name of up to 64 characters.'
}

$source = Join-Path $TemplatesRoot $Preset
$descriptor = Join-Path $source $catalogue[$Preset]
if ($Preset -ieq 'Van' -and !(Test-Path -LiteralPath (Join-Path $source 'SourceAssets/WorkshopVan.glb') -PathType Leaf)) {
    throw 'The original Workshop Van asset is missing from this template library.'
}
if (!(Test-Path -LiteralPath $descriptor -PathType Leaf)) { throw "Missing template $descriptor" }
$parent = [IO.Path]::GetFullPath($Destination)
$project = Join-Path $parent $Name
if (Test-Path -LiteralPath $project) { throw "A folder already exists at $project." }
$projectText = [IO.File]::ReadAllText($descriptor)
$identity = [regex]::Match($projectText, '(?m)^project_id\s*=\s*([0-9a-fA-F-]{36})\r?$')
if (!$identity.Success) { throw "Template project identity is missing in $descriptor." }
$oldId = $identity.Groups[1].Value
$newId = [Guid]::NewGuid().ToString()
$utf8 = [Text.UTF8Encoding]::new($false)

if (!('RenegadeTemplateHash' -as [type])) {
    Add-Type -TypeDefinition @'
public static class RenegadeTemplateHash {
    public static string Fnv1a64(byte[] bytes) {
        unchecked {
            ulong hash = 14695981039346656037UL;
            foreach (byte value in bytes) { hash ^= value; hash *= 1099511628211UL; }
            return "fnv1a64:" + hash.ToString("x16");
        }
    }
}
'@
}

[IO.Directory]::CreateDirectory($parent) | Out-Null
$pending = Join-Path $parent ('.renegade-new-' + $newId)
[IO.Directory]::CreateDirectory($pending) | Out-Null
try {
    foreach ($folder in 'Content', 'SourceAssets', 'VanTextures') {
        $item = Join-Path $source $folder
        if (Test-Path -LiteralPath $item -PathType Container) {
            Copy-Item -LiteralPath $item -Destination $pending -Recurse -Exclude '*.bak'
        }
    }
    $textTypes = '.rmeta', '.renegade-flow', '.rplayerprefab', '.rscripts'
    $replacedHashes = @{}
    foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $pending 'Content') -File -Recurse)) {
        if ($file.Extension -notin $textTypes) { continue }
        $before = [IO.File]::ReadAllText($file.FullName)
        if (!$before.Contains($oldId)) { continue }
        $path = $file.FullName.Substring($pending.Length + 1).Replace('\', '/')
        $rewritten = $before.Replace($oldId, $newId)
        [IO.File]::WriteAllText($file.FullName, $rewritten, $utf8)
        $replacedHashes[$path] = [RenegadeTemplateHash]::Fnv1a64([IO.File]::ReadAllBytes($file.FullName))
    }

    $registryName = 'AssetRegistry.renegade-assets'
    $registrySource = Join-Path $source $registryName
    if (Test-Path -LiteralPath $registrySource -PathType Leaf) {
        # Studio validates the canonical registry bytes. Preserve its formatting and ordering.
        $registry = [IO.File]::ReadAllText($registrySource).Replace($oldId, $newId)
        $registry = $registry.Replace("`r`n", "`n").TrimEnd() + "`n"
        $hashField = '("content_hash": ")fnv1a64:[0-9a-f]{16}(")'
        foreach ($path in $replacedHashes.Keys) {
            $encodedPath = ConvertTo-Json -InputObject $path -Compress
            $recordHash = $hashField + '(?=[^{}]*"path": ' + [regex]::Escape($encodedPath) + '\s*,)'
            $hashText = '${1}' + $replacedHashes[$path] + '${2}'
            $registry = [regex]::Replace($registry, $recordHash, $hashText)
        }
        [IO.File]::WriteAllText((Join-Path $pending $registryName), $registry, $utf8)
    }
    $content = [regex]::Replace($projectText.Replace($oldId, $newId), '(?m)^name = [^\r\n]*', {
        param($match)
        "name = $Name"
    })
    $leaf = $Name + '.renegade'
    [IO.File]::WriteAllText((Join-Path $pending $leaf), $content, $utf8)
    [IO.Directory]::Move($pending, $project)
    Join-Path $project $leaf
}
finally {
    if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending -Recurse -Force }
}
