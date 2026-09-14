# Gather/export or import/compile the game's native Unreal localization target.
[CmdletBinding()]
param(
    [ValidateSet('Gather', 'Export', 'Import', 'Compile', 'Update')]
    [string]$Action = 'Update',
    [string[]]$Cultures = @('en'),
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "Editor commandlet executable not found: $editor" }
$targetRoot = Join-Path $projectRoot 'Content/Localization/Game'
# Keep existing cultures when gathering; never silently discard their archives.
$existing = @()
if (Test-Path -LiteralPath $targetRoot) {
    $existing = @(Get-ChildItem -LiteralPath $targetRoot -Directory | Select-Object -ExpandProperty Name)
}
$allCultures = @(@('en') + $existing + $Cultures | Sort-Object -Unique)
foreach ($culture in $allCultures) {
    if ($culture -notmatch '^[a-zA-Z]{2,3}(-[a-zA-Z0-9]{2,8})*$') { throw "Invalid culture tag: $culture" }
}
$generatedRoot = Join-Path $projectRoot 'Saved/Localization'
New-Item -ItemType Directory -Force $generatedRoot | Out-Null
# Import edited PO files before Update: Export refreshes PO files from the archives.
$steps = if ($Action -eq 'Update') { @('Gather', 'Export', 'Compile') } else { @($Action) }
$configs = @()
foreach ($step in $steps) {
    $template = Get-Content -Raw -LiteralPath (Join-Path $projectRoot "Config/Localization/Game_$step.ini")
    $cultureLines = ($allCultures | ForEach-Object { "CulturesToGenerate=$_" }) -join "`n"
    $template = $template.Replace('CulturesToGenerate=en', $cultureLines)
    $generated = Join-Path $generatedRoot "Game_$step.ini"
    [IO.File]::WriteAllText($generated, $template, [Text.UTF8Encoding]::new($false))
    $configs += $generated
}
Push-Location $projectRoot
try {
    & $editor (Join-Path $projectRoot 'laby.uproject') '-run=GatherText' "-config=$($configs -join ';')" '-unattended' '-nop4' '-NullRHI' '-nosound' '-NoLiveCoding' '-DDC=InstalledNoZenLocalFallback' '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL' '-ini:Engine:[OnlineSubsystemEOS]:bEnabled=false'
    if ($LASTEXITCODE -ne 0) { throw "Localization $Action failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
# Unreal may emit UTF-16 JSON. Keep text catalogs readable in version control.
Get-ChildItem -LiteralPath $targetRoot -Recurse -File | Where-Object { $_.Extension -in '.manifest', '.archive', '.po' } | ForEach-Object {
    $catalog = [IO.File]::ReadAllText($_.FullName)
    $catalog = [regex]::Replace($catalog, '(?m)[ \t]+\r?$', '')
    [IO.File]::WriteAllText($_.FullName, ($catalog.TrimEnd() + "`n"), [Text.UTF8Encoding]::new($false))
}
Write-Host "Localization $Action completed for: $($allCultures -join ', ')"
