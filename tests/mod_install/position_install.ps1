# Vérifie l'installateur dans out/, avec les copies légitimes du jeu uniquement.
param([string] $GameDir, [string] $InstallerPath, [string] $PackageDir)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$GameDir) { $GameDir = Join-Path $root 'bin' }
if (!$InstallerPath) { $InstallerPath = Join-Path $root 'mods\position\install.ps1' }
if (!$PackageDir) { $PackageDir = Join-Path $root 'out\mods\position\paquet' }
$fixture = Join-Path $root 'out\tests\position-install'
New-Item -ItemType Directory -Force -Path $fixture | Out-Null
Copy-Item -LiteralPath (Join-Path $GameDir 'gamedll_x64_rwdi.dll'), (Join-Path $GameDir 'engine_x64_rwdi.dll') -Destination $fixture -Force
$install = $InstallerPath
$config = Join-Path $fixture 'dlmods.ini'
$before = "; configuration test`r`n[other]`r`nValue=12`r`n[loader]`r`nMods=dlcoop_reseau.dll;coop6.dll`r`n[env]`r`nCOOP6_Places=33`r`nSecretTest=preserver`r`n"
[IO.File]::WriteAllText($config, $before, (New-Object Text.UTF8Encoding($true)))
& $install -GameDir $fixture
$after = [IO.File]::ReadAllText($config)
if (!$after.Contains('Mods=dlcoop_reseau.dll;coop6.dll;position.dll') -or !$after.Contains('SecretTest=preserver') -or !$after.Contains('Value=12')) { throw 'Configuration not preserved' }
if (!(Test-Path (Join-Path $fixture 'position.dll')) -or !(Test-Path (Join-Path $fixture 'winmm.dll'))) { throw 'DLL missing after installation' }
& $install -GameDir $fixture
if ([IO.File]::ReadAllText($config) -ne $after) { throw 'Installation is not idempotent' }
& $install -GameDir $fixture -Uninstall
if ([IO.File]::ReadAllText($config) -ne $before -or (Test-Path (Join-Path $fixture 'position.dll'))) { throw 'Uninstallation is not reversible' }
$wrong = Join-Path $fixture 'winmm.dll'
[IO.File]::WriteAllBytes($wrong, [byte[]]@(1,2,3))
$refused = $false
try { & $install -GameDir $fixture } catch { $refused = $_.Exception.Message.Contains('Existing winmm.dll') }
if (!$refused -or [IO.File]::ReadAllText($config) -ne $before) { throw 'Unknown loader not cleanly rejected' }
Copy-Item -LiteralPath (Join-Path $PackageDir 'winmm.dll') -Destination $wrong -Force
$originalHash = (Get-FileHash -LiteralPath (Join-Path $GameDir 'gamedll_x64_rwdi.dll')).Hash
$gameFile = Join-Path $fixture 'gamedll_x64_rwdi.dll'
$stream = [IO.File]::OpenWrite($gameFile)
$stream.WriteByte(0)
$stream.Dispose()
$refused = $false
try { & $install -GameDir $fixture } catch { $refused = $_.Exception.Message.Contains('not the supported') }
if (!$refused -or [IO.File]::ReadAllText($config) -ne $before) { throw 'Unsupported game build not cleanly rejected' }
if ((Get-FileHash -LiteralPath (Join-Path $GameDir 'gamedll_x64_rwdi.dll')).Hash -ne $originalHash) { throw 'Source game copy was modified' }
Copy-Item -LiteralPath (Join-Path $GameDir 'gamedll_x64_rwdi.dll') -Destination $gameFile -Force
Remove-Item -LiteralPath $config, $wrong
& $install -GameDir $fixture
if ([IO.File]::ReadAllText($config) -notmatch '(?m)^Mods=position\.dll\r?$') { throw 'Fresh installation did not enable only the overlay.' }
if (-not ('PositionIniTest' -as [type])) {
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
using System.Text;
public static class PositionIniTest {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
    public static extern uint GetPrivateProfileString(string section, string key, string fallback, StringBuilder result, uint size, string path);
}
'@
}
$native = New-Object Text.StringBuilder 1024
[void][PositionIniTest]::GetPrivateProfileString('loader','Mods','',$native,1024,$config)
if ($native.ToString() -ne 'position.dll') { throw 'Windows INI API cannot read the fresh loader configuration.' }
& $install -GameDir $fixture -Uninstall
Write-Host 'RESULT: PASS (7 installer scenarios: preservation, idempotence, removal, unknown loader, unsupported build, fresh install, native Windows INI parsing)'
