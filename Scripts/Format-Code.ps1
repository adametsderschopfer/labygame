# Format project-owned source only. Use -Check for an optional read-only check.
[CmdletBinding()]
param([switch]$Check)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$formatter = Join-Path $projectRoot '.tools/clang-format-21.1.8/clang_format/data/bin/clang-format.exe'
$uncrustify = Join-Path $projectRoot '.tools/uncrustify-0.83.0/uncrustify.exe'
if (-not (Test-Path -LiteralPath $formatter)) {
    & (Join-Path $PSScriptRoot 'Install-Formatter.ps1')
}
if (-not (Test-Path -LiteralPath $uncrustify)) {
    & (Join-Path $PSScriptRoot 'Install-Uncrustify.ps1')
}

$sourceRoot = Join-Path $projectRoot 'Source'
$files = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
    Where-Object { $_.Extension -in '.h', '.hpp', '.cpp', '.cc', '.c', '.cs' -and
        $_.Name -notmatch '\.(generated\.h|gen\.cpp)$' } |
    Sort-Object FullName)

$uncrustifyConfig = Join-Path $projectRoot '.uncrustify.cfg'
$clangConfig = Join-Path $projectRoot '.clang-format'
$unformatted = @()
foreach ($file in $files) {
    # All stages run on a temporary file, even in -Check mode. Never pipe source
    # through PowerShell, which could change its encoding or line endings.
    # Preserve the extension so clang-format also recognizes C# build rules.
    $temporary = Join-Path ([System.IO.Path]::GetTempPath()) ("laby-format-$([Guid]::NewGuid())$($file.Extension)")
    try {
        Copy-Item -LiteralPath $file.FullName -Destination $temporary
        # Expand packed statements/macros before Uncrustify detects boundaries.
        & $formatter "--style=file:$clangConfig" -i $temporary
        if ($LASTEXITCODE -ne 0) { throw "clang-format normalization failed: $($file.FullName)" }
        $language = 'CPP'
        if ($file.Extension -eq '.cs') { $language = 'CS' }
        if ($file.Extension -eq '.c') { $language = 'C' }
        & $uncrustify -q -c $uncrustifyConfig -l $language --replace --no-backup $temporary
        if ($LASTEXITCODE -ne 0) { throw "Uncrustify failed: $($file.FullName)" }
        & $formatter "--style=file:$clangConfig" "--assume-filename=$($file.FullName)" -i $temporary
        if ($LASTEXITCODE -ne 0) { throw "clang-format failed: $($file.FullName)" }

        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $temporary).Hash) {
            if ($Check) {
                $unformatted += $file.FullName
            } else {
                Copy-Item -LiteralPath $temporary -Destination $file.FullName
            }
        }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
if ($Check) {
    if ($unformatted.Count -gt 0) {
        throw "Formatting required:`n$($unformatted -join [Environment]::NewLine)"
    }
    Write-Host "Formatting verified for $($files.Count) source files."
} else {
    Write-Host "Formatted $($files.Count) source files."
}
