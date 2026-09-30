<#
.SYNOPSIS
Starts a new game version in the project settings and changelog.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Set-Version.ps1 -Version 0.0.0.4

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Set-Version.ps1 -Version 0.0.0.4 -WhatIf
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+(?:\.\d+){2,3}$')]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$settingsPath = Join-Path $projectRoot 'Config/DefaultGame.ini'
$changelogPath = Join-Path $projectRoot 'CHANGELOG.txt'

function Get-FileEncoding {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        return [System.Text.UTF8Encoding]::new($true)
    }
    return [System.Text.UTF8Encoding]::new($false)
}

$settings = [System.IO.File]::ReadAllText($settingsPath)
$changelog = [System.IO.File]::ReadAllText($changelogPath)
$section = [regex]::Match($settings, '(?ms)^\[/Script/EngineSettings.GeneralProjectSettings\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)')
if (!$section.Success) { throw 'GeneralProjectSettings was not found in Config/DefaultGame.ini.' }

$body = $section.Groups['body']
$versionLines = [regex]::Matches($body.Value, '(?m)^ProjectVersion=([^\r\n]*)$')
if ($versionLines.Count -ne 1) { throw 'Expected exactly one ProjectVersion in GeneralProjectSettings.' }

$currentVersion = $versionLines[0].Groups[1].Value.Trim()
$heading = [regex]::Match($changelog, '\A(?<stage>[A-Za-z]+) (?<version>\d+(?:\.\d+){2,3})(?=\r?\n|\z)')
if (!$heading.Success) { throw 'Expected a version heading at the start of CHANGELOG.txt.' }
if ($heading.Groups['version'].Value -ne $currentVersion) {
    throw "ProjectVersion ($currentVersion) differs from the current changelog heading ($($heading.Groups['version'].Value))."
}
if ($Version -eq $currentVersion) { throw "Version $Version is already current." }
if ([regex]::IsMatch($changelog, "(?m)^[A-Za-z]+ $([regex]::Escape($Version))(?=\r?$)")) {
    throw "Version $Version already appears in CHANGELOG.txt."
}

$line = $versionLines[0]
$index = $body.Index + $line.Index
$updatedSettings = $settings.Substring(0, $index) + "ProjectVersion=$Version" + $settings.Substring($index + $line.Length)
$newline = if ($changelog.Contains("`r`n")) { "`r`n" } else { "`n" }
$newHeading = "$($heading.Groups['stage'].Value) $Version$newline$newline$(Get-Date -Format 'dd.MM.yyyy')$newline$newline"
$updatedChangelog = $newHeading + $changelog

if ($PSCmdlet.ShouldProcess('Config/DefaultGame.ini and CHANGELOG.txt', "Set game version to $Version")) {
    $settingsEncoding = Get-FileEncoding $settingsPath
    $changelogEncoding = Get-FileEncoding $changelogPath
    [System.IO.File]::WriteAllText($settingsPath, $updatedSettings, $settingsEncoding)
    try {
        [System.IO.File]::WriteAllText($changelogPath, $updatedChangelog, $changelogEncoding)
    }
    catch {
        [System.IO.File]::WriteAllText($settingsPath, $settings, $settingsEncoding)
        throw
    }
    Write-Host "Game version is now $Version. Add player-visible changes under the new CHANGELOG.txt heading."
}
