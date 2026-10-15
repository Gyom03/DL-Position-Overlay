# Installe/désinstalle l'overlay, en conservant les autres mods et leur configuration.
param([string] $GameDir, [switch] $Uninstall)
$ErrorActionPreference = 'Stop'

function Find-SteamGame {
    foreach ($key in 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 239140',
                         'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 239140') {
        $location = (Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location -and (Test-Path -LiteralPath (Join-Path $location 'DyingLightGame.exe'))) { return $location }
    }
    $steam = (Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (!$steam) { return $null }
    $libraries = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $vdf) {
        foreach ($match in [regex]::Matches([IO.File]::ReadAllText($vdf), '"path"\s+"([^"]+)"')) {
            $libraries += $match.Groups[1].Value.Replace('\\', '\')
        }
    }
    foreach ($library in $libraries) {
        $location = Join-Path $library 'steamapps\common\Dying Light'
        if (Test-Path -LiteralPath (Join-Path $location 'DyingLightGame.exe')) { return $location }
    }
    return $null
}

if (!$GameDir) { $GameDir = Find-SteamGame }
if (!$GameDir) { $GameDir = (Read-Host 'Dying Light folder (the folder containing DyingLightGame.exe)').Trim('"') }
if (!$GameDir -or !(Test-Path -LiteralPath $GameDir -PathType Container)) { throw 'Game folder not found.' }
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path.TrimEnd('\')
$packaged = Test-Path -LiteralPath (Join-Path $PSScriptRoot 'position.dll')
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$package = if ($packaged) { $PSScriptRoot } else { Join-Path $root 'out\mods\position\paquet' }
$config = Join-Path $GameDir 'dlmods.ini'
$dll = Join-Path $GameDir 'position.dll'
$running = Get-Process -Name DyingLightGame -ErrorAction SilentlyContinue | Where-Object {
    !$_.Path -or [IO.Path]::GetDirectoryName($_.Path) -ieq $GameDir
}
if ($running) { throw 'Close Dying Light before installing or uninstalling this mod.' }
$loader = Join-Path $GameDir 'winmm.dll'
$addLoader = $false
if (!$Uninstall) {
    $manifest = Get-Content -LiteralPath (Join-Path $package 'version155.json') -Raw | ConvertFrom-Json
    foreach ($name in 'gamedll_x64_rwdi.dll', 'engine_x64_rwdi.dll', 'filesystem_x64_rwdi.dll') {
        $actual = (Get-FileHash -LiteralPath (Join-Path $GameDir $name) -Algorithm SHA256).Hash
        if ($actual -ne $manifest.$name) { throw "$name is not the supported Steam 1.55 build. No changes made." }
    }
    $acceptedLoaders = @((Get-FileHash -LiteralPath (Join-Path $package 'winmm.dll')).Hash,
        '8DD33B078A843E82E54200ACCDB0A9E336DB7CE9E628D1BC13023E7D017674D1',
        '104446FD1D8303A59ED081C76F40436BE9F7F7332ED5129311B7F8B72DFA8353')
    if (Test-Path -LiteralPath $loader) {
        if ((Get-FileHash -LiteralPath $loader).Hash -notin $acceptedLoaders) {
            throw 'Existing winmm.dll is not a recognized mod loader. It has been kept unchanged. Use your existing loader for manual installation.'
        }
    } else { $addLoader = $true }
}
$exists = Test-Path -LiteralPath $config
$text = if ($exists) { [IO.File]::ReadAllText($config) } else { "[loader]`r`nMods=`r`n" }
$section = [regex]::Match($text, '(?ims)^\[loader\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)')
if (!$section.Success) { throw 'The [loader] section is missing from dlmods.ini. No changes made.' }
$entry = [regex]::Match($section.Groups['body'].Value, '(?im)^Mods\s*=([^\r\n]*)')
$mods = if ($entry.Success) { @($entry.Groups[1].Value.Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
        elseif ($addLoader) { @() } else { @('dlcoop_reseau.dll', 'coop6.dll') }
$mods = @($mods | Where-Object { [IO.Path]::GetFileName($_) -notmatch '^position(?:_1\.0\.1)?\.dll$' })
if (!$Uninstall) { $mods += 'position.dll' }
$line = 'Mods=' + ($mods -join ';')
$body = $section.Groups['body'].Value
if ($entry.Success) { $body = $body.Remove($entry.Index, $entry.Length).Insert($entry.Index, $line) }
else { $body = $line + "`r`n" + $body }
$updated = $text.Remove($section.Groups['body'].Index, $section.Groups['body'].Length).Insert($section.Groups['body'].Index, $body)
$backup = Join-Path $GameDir 'position_backup'
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
if ($exists) { Copy-Item -LiteralPath $config -Destination (Join-Path $backup "dlmods_$stamp.ini") }
if (Test-Path -LiteralPath $dll) { Copy-Item -LiteralPath $dll -Destination (Join-Path $backup "position_$stamp.dll") }
if (!$Uninstall) {
    New-Item -ItemType Directory -Path (Join-Path $GameDir 'CustomPaks') -Force | Out-Null
    if ($addLoader) { Copy-Item -LiteralPath (Join-Path $package 'winmm.dll') -Destination $loader }
    Copy-Item -LiteralPath (Join-Path $package 'position.dll') -Destination $dll -Force
}
# UTF-16 LE est lu nativement par les API INI Windows du chargeur, y compris la première section.
[IO.File]::WriteAllText($config, $updated, [Text.Encoding]::Unicode)
if ($Uninstall -and (Test-Path -LiteralPath $dll)) { Remove-Item -LiteralPath $dll }
if ($Uninstall -and (Test-Path -LiteralPath (Join-Path $GameDir 'position_1.0.1.dll'))) {
    Remove-Item -LiteralPath (Join-Path $GameDir 'position_1.0.1.dll')
}
Write-Host $(if ($Uninstall) { 'Position overlay uninstalled. The shared loader and other mods have been kept.' }
    else { 'Position overlay installed. Put .pak files in CustomPaks and start Dying Light normally. F8 shows or hides the coordinates.' })
Write-Host $line
