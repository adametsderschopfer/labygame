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

function Visit-LocalizationNode {
    param($Node, [string]$ParentNamespace, [scriptblock]$VisitChild)

    $namespace = if ($Node.Namespace) {
        if ($ParentNamespace) { "$ParentNamespace.$($Node.Namespace)" } else { [string]$Node.Namespace }
    } else { $ParentNamespace }
    foreach ($child in $Node.Children) { & $VisitChild $namespace $child }
    foreach ($subnamespace in $Node.Subnamespaces) {
        Visit-LocalizationNode $subnamespace $namespace $VisitChild
    }
}

function Read-LocalizationJson {
    param([string]$Path)

    return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-ManifestSources {
    $sources = @{}
    $manifest = Read-LocalizationJson (Join-Path $targetRoot 'Game.manifest')
    $visitor = {
        param($namespace, $child)
        foreach ($key in $child.Keys) { $sources["$namespace,$($key.Key)"] = [string]$child.Source.Text }
    }.GetNewClosure()
    Visit-LocalizationNode $manifest '' $visitor
    return $sources
}

function Get-ArchiveTranslations {
    param([string]$Culture)

    $translations = @{}
    $archive = Read-LocalizationJson (Join-Path $targetRoot "$Culture/Game.archive")
    $visitor = {
        param($namespace, $child)
        $translations["$namespace,$($child.Key)"] = [string]$child.Translation.Text
    }.GetNewClosure()
    Visit-LocalizationNode $archive '' $visitor
    return $translations
}

function Sync-ArchiveSources {
    param([string[]]$CultureNames)

    $sources = Get-ManifestSources
    foreach ($culture in $CultureNames) {
        $path = Join-Path $targetRoot "$culture/Game.archive"
        if (!(Test-Path -LiteralPath $path)) { continue }
        $archive = Read-LocalizationJson $path
        $state = @{ Changed = 0 }
        $visitor = {
            param($namespace, $child)
            $key = "$namespace,$($child.Key)"
            if ($sources.ContainsKey($key) -and $child.Source.Text -cne $sources[$key]) {
                $child.Source.Text = $sources[$key]
                $state.Changed++
            }
        }.GetNewClosure()
        Visit-LocalizationNode $archive '' $visitor
        if ($state.Changed) {
            [IO.File]::WriteAllText($path, (($archive | ConvertTo-Json -Depth 100) + "`n"), [Text.UTF8Encoding]::new($false))
            Write-Host "Aligned $($state.Changed) $culture archive sources with the game manifest."
        }
    }
}

function Escape-PoString {
    param([string]$Value)

    return $Value.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
}

# Keep existing cultures when gathering; never silently discard their archives.
$existing = @()
if (Test-Path -LiteralPath $targetRoot) {
    $existing = @(Get-ChildItem -LiteralPath $targetRoot -Directory | Select-Object -ExpandProperty Name)
}
$allCultures = @(@('en') + $existing + $Cultures | Sort-Object -Unique)
foreach ($culture in $allCultures) {
    if ($culture -notmatch '^[a-zA-Z]{2,3}(-[a-zA-Z0-9]{2,8})*$') { throw "Invalid culture tag: $culture" }
}
$previousSources = if ($Action -eq 'Update') { Get-ManifestSources } else { @{} }
$previousTranslations = @{}
if ($Action -eq 'Update') {
    foreach ($culture in $allCultures | Where-Object { $_ -ne 'en' }) {
        $previousTranslations[$culture] = Get-ArchiveTranslations $culture
    }
}
if ($Action -in @('Update', 'Compile')) { Sync-ArchiveSources $allCultures }
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
    & $editor (Join-Path $projectRoot 'laby.uproject') '-run=GatherText' "-config=$($configs -join ';')" '-unattended' '-nop4' '-NullRHI' '-nosound' '-NoLiveCoding' '-DisablePlugins=ModelContextProtocol' '-DDC=InstalledNoZenLocalFallback' '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL' '-ini:Engine:[OnlineSubsystemEOS]:bEnabled=false'
    if ($LASTEXITCODE -ne 0) { throw "Localization $Action failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
# Unreal may emit UTF-16 JSON. Keep text catalogs readable in version control.
Get-ChildItem -LiteralPath $targetRoot -Recurse -File | Where-Object { $_.Extension -in '.manifest', '.archive', '.po' } | ForEach-Object {
    $catalog = [IO.File]::ReadAllText($_.FullName)
    $catalog = [regex]::Replace($catalog, '(?m)[ \t]+\r?$', '')
    [IO.File]::WriteAllText($_.FullName, ($catalog.TrimEnd() + "`n"), [Text.UTF8Encoding]::new($false))
}
if ($Action -eq 'Import') { Sync-ArchiveSources $allCultures }
if ($Action -eq 'Update') {
    $currentSources = Get-ManifestSources
    foreach ($culture in $allCultures | Where-Object { $_ -ne 'en' }) {
        $poPath = Join-Path $targetRoot "$culture/Game.po"
        $po = [IO.File]::ReadAllText($poPath, [Text.Encoding]::UTF8)
        $translations = $previousTranslations[$culture]
        $pattern = '(?m)^(?<context>msgctxt "(?<key>[^"]+)"\r?\nmsgid "(?:[^"\\]|\\.)*"\r?\n)msgstr ""$'
        $po = [regex]::Replace($po, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{
            param($match)
            $key = $match.Groups['key'].Value
            if (!$previousSources.ContainsKey($key) -or !$currentSources.ContainsKey($key) -or
                $previousSources[$key] -cne $currentSources[$key] -or !$translations.ContainsKey($key) -or
                !$translations[$key]) { return $match.Value }
            return $match.Groups['context'].Value + 'msgstr "' + (Escape-PoString $translations[$key]) + '"'
        })
        [IO.File]::WriteAllText($poPath, $po, [Text.UTF8Encoding]::new($false))
    }
}
if ($Action -in @('Update', 'Compile')) {
    foreach ($culture in $allCultures | Where-Object { $_ -ne 'en' }) {
        $poPath = Join-Path $targetRoot "$culture/Game.po"
        $po = [IO.File]::ReadAllText($poPath)
        $entries = [regex]::Matches($po, '(?m)^msgctxt ')
        $missing = [regex]::Matches($po, '(?m)^msgstr ""\r?$').Count - 1 # Header has an empty msgstr.
        if ($entries.Count -eq 0 -or $missing -gt 0) {
            throw "$culture has $missing untranslated entries in Game.po. Translate them, then run Import and Compile."
        }
    }
}
Write-Host "Localization $Action completed for: $($allCultures -join ', ')"
