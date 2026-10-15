# Prépare une release autonome : uniquement nos binaires et des métadonnées d'identification.
$ErrorActionPreference = 'Stop'
$version = '1.0.3'
$destination = Join-Path $PSScriptRoot "out\DLPositionOverlay_$version"
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'out\build\position.dll'), (Join-Path $PSScriptRoot 'out\build\winmm.dll'),
    (Join-Path $PSScriptRoot 'install.ps1'), (Join-Path $PSScriptRoot 'Install.cmd'), (Join-Path $PSScriptRoot 'Uninstall.cmd'),
    (Join-Path $PSScriptRoot 'README.md'), (Join-Path $PSScriptRoot 'version155.json') -Destination $destination
$hashes = Get-ChildItem -LiteralPath $destination -File | Where-Object Name -ne 'SHA256SUMS.txt' | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
}
[IO.File]::WriteAllLines((Join-Path $destination 'SHA256SUMS.txt'), [string[]]$hashes, [Text.Encoding]::ASCII)
$zip = Join-Path $PSScriptRoot "out\DLPositionOverlay_$version.zip"
Compress-Archive -Path (Join-Path $destination '*') -DestinationPath $zip -Force
Get-Item -LiteralPath $zip | Select-Object FullName, Length
