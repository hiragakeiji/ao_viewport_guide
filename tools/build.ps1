param(
    [ValidateSet("Debug","Release")] [string]$Config = "Release"
)

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"

cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build done."
