# Pinned official Windows x64 release; no administrator access needed.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$toolDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) '.tools/uncrustify-0.83.0'
$formatter = Join-Path $toolDirectory 'uncrustify.exe'
if (Test-Path -LiteralPath $formatter) {
    & $formatter --version
    if ($LASTEXITCODE -ne 0) { throw 'The installed Uncrustify could not run.' }
    return
}

$url = 'https://github.com/uncrustify/uncrustify/releases/download/uncrustify-0.83.0/uncrustify-0.83.0_f-win64.zip'
$expectedHash = 'af017849b2002bb1d23ec72959450adeac00a274612e62029b175a37f4178805'
New-Item -ItemType Directory -Path $toolDirectory -Force | Out-Null
$archive = Join-Path $toolDirectory 'package.zip'
Invoke-WebRequest -Uri $url -OutFile $archive
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Uncrustify download checksum mismatch.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $toolDirectory -Force
Remove-Item -LiteralPath $archive
$binary = Get-ChildItem -LiteralPath $toolDirectory -Recurse -File -Filter 'uncrustify.exe' |
    Select-Object -First 1
if (-not $binary) { throw 'The archive does not contain uncrustify.exe.' }
if ($binary.FullName -ne $formatter) {
    Copy-Item -LiteralPath $binary.FullName -Destination $formatter
}
& $formatter --version
if ($LASTEXITCODE -ne 0) { throw 'Uncrustify installation failed.' }
