# Install the pinned Windows x64 formatter locally; no Python or admin access needed.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$toolDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) '.tools/clang-format-21.1.8'
$formatter = Join-Path $toolDirectory 'clang_format/data/bin/clang-format.exe'
if (Test-Path -LiteralPath $formatter) {
    & $formatter --version
    if ($LASTEXITCODE -ne 0) { throw 'The installed formatter could not run.' }
    return
}

$url = 'https://files.pythonhosted.org/packages/69/7a/c49c8af9135c6a6dfb9cd103328ba7b6551643dce71b57e58ea940884015/clang_format-21.1.8-py2.py3-none-win_amd64.whl'
$expectedHash = 'a7606da55e31ebf5b63dd75800392e6cca7c595a74100c2cebcda2d742130732'
New-Item -ItemType Directory -Path $toolDirectory -Force | Out-Null
$archive = Join-Path $toolDirectory 'package.zip'
Invoke-WebRequest -Uri $url -OutFile $archive
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Formatter download checksum mismatch.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $toolDirectory -Force
Remove-Item -LiteralPath $archive
& $formatter --version
if ($LASTEXITCODE -ne 0) { throw 'Formatter installation failed.' }
