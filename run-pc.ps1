#requires -Version 7.0
# Launch the prepared baseline without invoking the build or changing user saves.
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'port/build/windows-x64-release/darkchronicle.exe'
$run = Join-Path $PSScriptRoot 'port/run'
if (!(Test-Path -LiteralPath $exe)) { throw 'Build windows-x64-release first.' }
if (!(Test-Path -LiteralPath (Join-Path $run 'data.zip'))) { throw 'Extract the PAL disc into port/run/data first.' }
Push-Location (Split-Path $exe)
try {
    & $exe --run-dir $run @args
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $code
