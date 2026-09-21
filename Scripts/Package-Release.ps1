param(
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8',
    [switch]$Describe
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $settings = Get-Content -LiteralPath 'Config/DefaultGame.ini' -Raw
    $section = [regex]::Match($settings, '(?ms)^\[/Script/EngineSettings.GeneralProjectSettings\]\s*\r?\n(?<body>.*?)(?=^\[|\z)').Groups['body'].Value
    $gameName = [regex]::Match($section, '(?m)^ProjectName=([^\r\n]+)').Groups[1].Value.Trim()
    $version = [regex]::Match($section, '(?m)^ProjectVersion=([^\r\n]+)').Groups[1].Value.Trim()
    if (!$gameName -or !$version) { throw 'Set ProjectName and ProjectVersion in Config/DefaultGame.ini.' }
    foreach ($part in @($gameName, $version)) {
        if ($part -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
            throw 'Release name and version must contain only ASCII letters, digits, dots, underscores and hyphens.'
        }
    }
    $revision = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the source revision.' }
    $identifier = git rev-parse --short=8 HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the release identifier.' }
    $baseName = "$gameName-$version-$identifier"
    $releaseRoot = Join-Path $projectRoot "Saved/Builds/$baseName"
    $gameZip = Join-Path $releaseRoot "$baseName.zip"
    $sourceZip = Join-Path $releaseRoot "$baseName-Project.zip"
    if ($Describe) {
        [pscustomobject]@{ Game = $gameZip; Project = $sourceZip; Revision = $revision }
        return
    }

    # A commit identifies the exact source in both the binary and source archive.
    $changes = @(git status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the working tree.' }
    if ($changes.Count) { throw 'Commit project changes before packaging so the release identifier matches its source.' }
    if (Test-Path -LiteralPath $releaseRoot) { throw "Release directory already exists; existing releases are never overwritten: $releaseRoot" }
    $uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
    if (!(Test-Path -LiteralPath $uat)) { throw "RunUAT not found: $uat" }
    $project = Join-Path $projectRoot 'laby.uproject'
    $archiveRoot = Join-Path $releaseRoot 'Windows'
    New-Item -ItemType Directory -Path $releaseRoot | Out-Null
    $cookOptions = '-SkipZenStore -ddc=InstalledNoZenLocalFallback -ini:Engine:[OnlineSubsystemEOS]:bEnabled=false -ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL -ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=false'
    # Build the separate game target; never overwrite an open editor's modules.
    # EOS/MCP overrides apply to the cook commandlet only, not the packaged game.
    & $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Shipping `
        -build -skipbuildeditor -cook "-AdditionalCookerOptions=$cookOptions" `
        -map=/Game/Maps/Maze -stage -pak -iostore -compressed -prereqs -package `
        -archive "-archivedirectory=$archiveRoot" -nodebuginfo -unattended -utf8output `
        2>&1 | Tee-Object -FilePath (Join-Path $releaseRoot 'Packaging.log')
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed. See Packaging.log; no release ZIPs were created.' }
    $currentRevision = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot verify the source revision after packaging.' }
    $changes = @(git status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0 -or $changes.Count -or $currentRevision -ne $revision) {
        throw 'Source changed during packaging. No release ZIPs were created; package the new commit separately.'
    }
    if (!(Test-Path -LiteralPath (Join-Path $archiveRoot 'laby.exe'))) { throw 'Packaged launcher is missing.' }
    @"
$gameName $version / Windows x64 / Shipping
Source revision: $revision
Extract the entire ZIP, then run laby.exe. Keep Engine and laby folders together.
If Visual C++ runtime is missing, install Engine/Extras/Redist/en-us/vc_redist.x64.exe.
EOS internet rooms require configured project credentials.
Build and packaging succeeded. Game, Play and automated tests were not launched.
"@ | Set-Content -LiteralPath (Join-Path $archiveRoot 'BUILD-INFO.txt') -Encoding UTF8

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::Open($gameZip, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $archiveRoot -Recurse -File) {
            $relative = $file.FullName.Substring($archiveRoot.Length + 1).Replace('\', '/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zip, $file.FullName, "$gameName/$relative", [System.IO.Compression.CompressionLevel]::Optimal
            ) | Out-Null
        }
    } finally { $zip.Dispose() }
    git archive --format=zip --prefix=laby/ "--output=$sourceZip" $revision
    if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
    @($gameZip, $sourceZip) | Get-FileHash -Algorithm SHA256 | ForEach-Object {
        "$($_.Hash)  $([System.IO.Path]::GetFileName($_.Path))"
    } | Set-Content -LiteralPath (Join-Path $releaseRoot 'SHA256SUMS.txt')
    Set-Content -LiteralPath (Join-Path $projectRoot 'Saved/Builds/latest-shipping-path.txt') $releaseRoot
    Get-Item -LiteralPath $gameZip, $sourceZip | Select-Object FullName, Length
} finally { Pop-Location }
