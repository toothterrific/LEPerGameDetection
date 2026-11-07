# Copies Release binaries into dist\ and zips them for distribution (manual install or ME3Tweaks listing)
param(
    [string]$Version = "2.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "build\out"
$distDir = Join-Path $root "dist"
$stagingName = "MELEPerGamePresence_v$Version"
$staging = Join-Path $distDir $stagingName

$asi = Join-Path $outDir "MELEPerGamePresence.asi"
if (-not (Test-Path $asi)) {
    Write-Error "MELEPerGamePresence.asi not found. Build first: cmake --build build --config Release"
}

New-Item -ItemType Directory -Force -Path $staging | Out-Null

Copy-Item $asi (Join-Path $staging "MELEPerGamePresence.asi") -Force
Copy-Item (Join-Path $root "MELEPerGamePresence.ini.example") (Join-Path $staging "MELEPerGamePresence.ini") -Force
Copy-Item (Join-Path $root "dist\INSTALL.txt") (Join-Path $staging "INSTALL.txt") -Force

$outFile = Join-Path $distDir "$stagingName.zip"
if (Test-Path $outFile) { Remove-Item $outFile -Force }
Compress-Archive -Path $staging -DestinationPath $outFile
Write-Output "Created: $outFile"
