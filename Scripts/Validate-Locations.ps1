# Static content/config validation only. Does not start Unreal, Play, tests or a trace.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$config = Get-Content -LiteralPath (Join-Path $projectRoot 'Config/DefaultGame.ini')
$definitions = @($config | Where-Object { $_ -match '^\+Locations=' })
if ($definitions.Count -eq 0) { throw 'No location definitions found.' }
$maps = @{}
$assetCount = 0
foreach ($definition in $definitions) {
    if ($definition -notmatch 'Map="(?<map>/Game/[^" ]+)"') { throw "Invalid location map: $definition" }
    $mapPath = $Matches.map
    if ($maps.ContainsKey($mapPath)) { throw "Duplicate location: $mapPath" }
    $maps[$mapPath] = $true
    if ($definition -notmatch 'Mode=(Whole|Procedural|WorldPartition)') { throw "Invalid location mode: $mapPath" }
    $paths = [regex]::Matches($definition, '"(?<path>/Game/[^" ]+)"')
    foreach ($match in $paths) {
        $objectPath = $match.Groups['path'].Value
        $package = ($objectPath -split '\.')[0]
        $extension = if ($objectPath -eq $mapPath) { '.umap' } else { '.uasset' }
        $relative = 'Content/' + $package.Substring('/Game/'.Length) + $extension
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $relative) -PathType Leaf)) {
            throw "Missing location resource: $objectPath ($relative)"
        }
        $assetCount++
    }
}
Write-Output "Validated $($definitions.Count) location definitions and $assetCount package references."
