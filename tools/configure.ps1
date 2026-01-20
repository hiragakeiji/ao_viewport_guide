param(
    [string]$MayaLocation = "C:\Program Files\Autodesk\Maya2025",
    [ValidateSet("Debug","Release")] [string]$Config = "Release"
)

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"

# buildを作り直す（事故防止）
if (Test-Path $build) { Remove-Item -Recurse -Force $build }
New-Item -ItemType Directory -Force -Path $build | Out-Null

cmake -S $root -B $build -G "Visual Studio 18 2026" -A x64 -DMAYA_LOCATION="$MayaLocation"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Configure done. Build dir: $build"
Write-Host "Next: .\tools\build.ps1 -Config $Config"
