# Export an existing trace; this script never starts the editor, Play, or a capture.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$TraceFile,
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8',
    [switch]$IncludeEvents
)
$ErrorActionPreference = 'Stop'
$tracePath = (Resolve-Path -LiteralPath $TraceFile).Path
if ([IO.Path]::GetExtension($tracePath) -ne '.utrace') { throw 'Expected a .utrace file' }
$insights = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealInsights.exe'
if (-not (Test-Path -LiteralPath $insights)) { throw "Missing Unreal Insights: $insights" }
$projectRoot = Split-Path $PSScriptRoot -Parent
$reportPath = Join-Path $projectRoot ('Saved/Profiling/Reports/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $reportPath -Force | Out-Null
$stats = (Join-Path $reportPath 'timers.csv').Replace('\', '/')
$commands = @('TimingInsights.ExportTimerStatistics "' + $stats + '"')
if ($IncludeEvents) {
    $events = (Join-Path $reportPath 'events.csv').Replace('\', '/')
    $commands += 'TimingInsights.ExportTimingEvents "' + $events + '"'
}
$responseFile = Join-Path $reportPath 'export.rsp'
[IO.File]::WriteAllLines($responseFile, $commands)
$arguments = @(
    ('-OpenTraceFile="' + $tracePath + '"'),
    '-AutoQuit', '-NoUI',
    ('-ABSLOG="' + (Join-Path $reportPath 'export.log') + '"'),
    ('-ExecOnAnalysisCompleteCmd="@=' + $responseFile + '"')
)
$process = Start-Process -FilePath $insights -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $stats)) {
    throw "Insights export failed; inspect $reportPath/export.log"
}
if ($IncludeEvents -and -not (Test-Path -LiteralPath $events)) { throw "Missing events export: $events" }
Write-Output "Insights report: $reportPath"
