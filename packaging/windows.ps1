# Builds a Windows release zip containing the single kr8.exe and documentation.
# Run from a Visual Studio developer prompt with cmake on PATH.

param(
    [string]$Configuration = "Release",
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

cmake --preset windows-vs2026
cmake --build --preset "windows-vs2026-$($Configuration.ToLower())"

$binary = Join-Path $root "build/windows-vs2026/$Configuration/kr8.exe"
if (!(Test-Path $binary)) {
    throw "missing $binary"
}

$stage = Join-Path $root "$OutDir/kr8-0.2.3-win64"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item $binary $stage
Copy-Item README.md, LICENSE, THIRD_PARTY_NOTICES.md $stage
Copy-Item docs, examples $stage -Recurse

$archive = Join-Path $root "$OutDir/kr8-0.2.3-win64.zip"
if (Test-Path $archive) { Remove-Item -Force $archive }
Compress-Archive -Path "$stage/*" -DestinationPath $archive
Write-Output "wrote $archive"
