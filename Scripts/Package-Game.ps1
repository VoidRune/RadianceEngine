param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Name = "RadianceGame"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "bin\windows-x86_64\Game"
$output = Join-Path $root "build\Package"
$folder = "$Name-$Version-win64"
$stage = Join-Path $output $folder
$zip = Join-Path $output "$folder.zip"

$files = @(
    "Game.exe",
    "GameNetworkingSockets.dll",
    "libcrypto-3-x64.dll",
    "libprotobuf.dll",
    "steam_api64.dll"
)

foreach ($file in $files) {
    if (-not (Test-Path (Join-Path $source $file))) {
        throw "Missing $file in $source - build the Dist configuration first"
    }
}

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

foreach ($file in $files) {
    Copy-Item (Join-Path $source $file) $stage
}
Copy-Item (Join-Path $source "res") $stage -Recurse
Copy-Item (Join-Path $root "LICENSE") $stage

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $true)

$size = (Get-Item $zip).Length / 1MB
Write-Host ("Packaged {0} ({1:N1} MB)" -f $zip, $size)
