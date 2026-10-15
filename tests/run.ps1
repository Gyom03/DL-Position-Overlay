# Exécute le rendu WARP et, si demandé, les tests de capture sur les DLL de l'utilisateur.
param([string] $GameDir,
    [string] $VcVars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root 'out\tests'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
foreach ($name in 'position_dxgi', 'position_capture_host') {
    $source = Join-Path $PSScriptRoot "mod_install\$name.cpp"
    cmd /c "`"$VcVars`" x64 -vcvars_ver=14.16 >nul 2>nul && cd /d `"$destination`" && cl /nologo /O2 /MT /std:c++17 /EHsc /W4 /utf-8 `"$source`" /Fe:$name.exe /link user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib"
    if ($LASTEXITCODE -ne 0) { throw "Test build failed: $name" }
}
& (Join-Path $destination 'position_dxgi.exe')
if ($LASTEXITCODE -ne 0) { throw 'DirectX pixel test failed.' }
$loaderTest = Join-Path $destination 'loader'
New-Item -ItemType Directory -Force -Path $loaderTest | Out-Null
$loaderHost = Join-Path $PSScriptRoot 'loader_host.cpp'
$marker = Join-Path $PSScriptRoot 'loader_marker.cpp'
cmd /c "`"$VcVars`" x64 -vcvars_ver=14.16 >nul 2>nul && cd /d `"$loaderTest`" && cl /nologo /O2 /MT /utf-8 `"$loaderHost`" /Fe:loader_host.exe && cl /nologo /O2 /MT /utf-8 /LD `"$marker`" /Fe:loader_marker.dll"
if ($LASTEXITCODE -ne 0) { throw 'Loader fixture build failed.' }
Copy-Item -LiteralPath (Join-Path $root 'out\build\winmm.dll') -Destination (Join-Path $loaderTest 'winmm.dll') -Force
[IO.File]::WriteAllText((Join-Path $loaderTest 'dlmods.ini'), "[loader]`r`nMods=loader_marker.dll`r`n", [Text.Encoding]::Unicode)
for ($i=0; $i -lt 10; $i++) {
    & (Join-Path $loaderTest 'loader_host.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Loader startup test failed.' }
}
if ($GameDir) {
    $game = Join-Path $GameDir 'gamedll_x64_rwdi.dll'
    $engine = Join-Path $GameDir 'engine_x64_rwdi.dll'
    $hostExe = Join-Path $destination 'position_capture_host.exe'
    & $hostExe $game $engine
    if ($LASTEXITCODE -ne 0) { throw 'Coordinate sampling tests failed.' }
    foreach ($mode in 'valid','invalid') {
        & $hostExe $game $engine (Join-Path $root 'out\build\position.dll') $mode
        if ($LASTEXITCODE -ne 0) { throw "DLL loading test failed: $mode" }
    }
    & (Join-Path $root 'package.ps1')
    $package = Join-Path $root 'out\DLPositionOverlay_1.0.3'
    & (Join-Path $PSScriptRoot 'mod_install\position_install.ps1') -GameDir $GameDir -InstallerPath (Join-Path $package 'install.ps1') -PackageDir $package
} else { Write-Host 'Game-independent tests passed. Supply -GameDir to also test coordinate sampling and installation in a temporary folder.' }
