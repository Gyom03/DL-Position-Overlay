# Compilation autonome du mod et du chargeur ; aucun fichier du jeu requis.
param(
    [string] $VcVars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)
$ErrorActionPreference = 'Stop'
$destination = Join-Path $PSScriptRoot 'out\build'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$mod = Join-Path $PSScriptRoot 'mods\position\PositionMod.cpp'
$loader = Join-Path $PSScriptRoot 'loader\dllmain.cpp'
if (!(Test-Path -LiteralPath $VcVars)) { throw 'MSVC Build Tools not found. Supply -VcVars with the path to vcvarsall.bat (v141/14.16 required).' }
cmd /c "`"$VcVars`" x64 -vcvars_ver=14.16 >nul 2>nul && cd /d `"$destination`" && cl /nologo /O2 /MT /std:c++17 /EHsc /W4 /utf-8 /LD `"$mod`" /Fe:position.dll /link /Brepro user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib"
if ($LASTEXITCODE -ne 0) { throw 'Position overlay build failed.' }
cmd /c "`"$VcVars`" x64 -vcvars_ver=14.16 >nul 2>nul && cd /d `"$destination`" && cl /nologo /O2 /MT /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /LD `"$loader`" /Fe:winmm.dll /link /Brepro"
if ($LASTEXITCODE -ne 0) { throw 'Mod loader build failed.' }
Get-Item -LiteralPath (Join-Path $destination 'position.dll'), (Join-Path $destination 'winmm.dll') | Select-Object Name, Length
