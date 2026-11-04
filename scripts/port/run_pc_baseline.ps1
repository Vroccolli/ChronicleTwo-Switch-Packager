#requires -Version 7.0
param(
    [string]$Executable = 'port/build/windows-x64-release/darkchronicle.exe',
    [string]$RunDirectory = 'port/run',
    [string]$OutputDirectory,
    [int]$TimeoutSeconds = 300,
    [string[]]$Cases = @('title', 'costume', 'opening', 'town', 'dungeon')
)
$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
function Resolve-ProjectPath([string]$Path) {
    if (![IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $checkout $Path }
    [IO.Path]::GetFullPath($Path)
}
$exe = Resolve-ProjectPath $Executable
$run = Resolve-ProjectPath $RunDirectory
if (!$OutputDirectory) { $OutputDirectory = '.kazarov/baseline/' + (Get-Date -Format 'yyyyMMdd-HHmmss') }
$output = Resolve-ProjectPath $OutputDirectory
if (!(Test-Path -LiteralPath $exe)) { throw "Build the PC executable first: $exe" }
if (!(Test-Path -LiteralPath (Join-Path $run 'data.zip'))) { throw "Extract the PAL disc into $run first." }
# Replays must use fresh saves and configuration for comparable results.
if (Test-Path -LiteralPath $output) { throw "Choose a new output directory: $output" }
$definitions = @(
    @{ Name = 'title'; Frames = 3900; Input = 'sweep-title.input' },
    @{ Name = 'costume'; Frames = 5200; Input = 'pc-baseline-newgame.input' },
    @{ Name = 'opening'; Frames = 4200; Input = 'monica-opening-combat.input'; Jump = 'opening' },
    @{ Name = 'town'; Frames = 480; Input = 'area-town-walk.input'; Jump = 'town:10' },
    @{ Name = 'dungeon'; Frames = 2400; Input = 'jump-dungeon-floor1.input'; Jump = 'dungeon:0:1' }
)
foreach ($name in $Cases) {
    if ($name -notin $definitions.Name) { throw "Unknown case: $name" }
}
New-Item -ItemType Directory -Path $output | Out-Null
$results = @()
foreach ($case in $definitions | Where-Object Name -In $Cases) {
    $directory = Join-Path $output $case.Name
    New-Item -ItemType Directory -Path $directory | Out-Null
    $capture = Join-Path $directory 'frame.png'
    $arguments = @('--offscreen', '--window', '640x480', '--run-dir', $run,
        '--save', (Join-Path $directory 'saves'), '--frames', [string]$case.Frames,
        '--screenshot', $capture, '--input', (Join-Path $checkout "tools/input/$($case.Input)"))
    if ($case.Jump) { $arguments += @('--jump', $case.Jump, '--fast-load') }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Write-Host "baseline: $($case.Name) ($($case.Frames) frames)"
    $process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) `
        -ArgumentList ($arguments | ForEach-Object { '"' + $_ + '"' }) `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $directory 'stdout.log') `
        -RedirectStandardError (Join-Path $directory 'stderr.log')
    $timedOut = !$process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) { $process.Kill($true); $process.WaitForExit() }
    $result = [ordered]@{
        case = $case.Name; frames = $case.Frames; exit_code = $process.ExitCode
        timed_out = $timedOut; screenshot = (Test-Path -LiteralPath $capture)
        seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 2); arguments = $arguments
    }
    $process.Dispose()
    $results += $result
    $results | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $output 'results.json')
    Write-Host "baseline: $($case.Name): exit $($result.exit_code), screenshot $($result.screenshot), $($result.seconds)s"
}
Write-Host "baseline: evidence in $output; screenshots require visual review"
if (@($results | Where-Object { $_.exit_code -ne 0 -or $_.timed_out -or !$_.screenshot }).Count) { exit 1 }
