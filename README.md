# Dying Light Position Overlay

A small in-game overlay for **Dying Light Steam 1.55 (64-bit)**. Shows the local player's world
coordinates and the active camera's world coordinates in the top-left corner.

- **F8** shows or hides the panel. It is visible by default in a game session.
- **Player** coordinates are white; **Camera** coordinates are cyan.
- X, Y and Z use the game's world axes, with three decimal places. Missing values display `--`.
- All overlay text, installer messages and runtime logs are in English.
- Draws through Direct3D 11 at frame presentation. No launcher, network connection or account required.

## Install

1. Download the ZIP from [Releases](https://github.com/Gyom03/DL-Position-Overlay/releases/latest).
2. **Extract the entire ZIP** into a folder. Do not run it from inside the ZIP viewer.
3. Close Dying Light, then double-click **Install.cmd**. The installer finds the Steam game folder,
   or asks for it if it cannot find it automatically.
4. Start the game normally through Steam. Load a save; press **F8** to hide or show the overlay.

The installer verifies the supported game build, adds `position.dll` to `dlmods.ini`, and installs
the included `winmm.dll` mod loader if needed. Existing mods and environment settings are preserved.
Backups are stored in `position_backup` inside the game folder. An unrecognized existing `winmm.dll`
is kept unchanged and installation stops; use your current loader for manual installation instead.

To select a folder explicitly, run this in PowerShell from the extracted release folder:

```powershell
.\install.ps1 -GameDir 'D:\SteamLibrary\steamapps\common\Dying Light'
```

Optional: add `-nologos` to Dying Light's Steam launch options to skip the intro logos.

## Manual installation

With the game closed, copy `position.dll` and the included `winmm.dll` next to `DyingLightGame.exe`.
Create `dlmods.ini` in that folder:

```ini
[loader]
Mods=position.dll
```

If this loader is already installed, keep it and append `;position.dll` to its existing `Mods=` line.
Do not replace another mod's loader or overwrite an existing configuration with this example.
DLML (`dsound.dll`) does not load this DLL on its own; use the included loader or another DLL loader
that supports loading `position.dll`.

**DLCoop users:** its launcher rewrites `dlmods.ini` and removes additional mod entries. After using
the launcher, install this overlay again and start Dying Light directly through Steam. The coordinate
overlay can share the existing DLCoop loader and preserves its other mod entries.

## Uninstall

Close the game and double-click **Uninstall.cmd**, or run `install.ps1 -Uninstall -GameDir '<folder>'`.
This removes the overlay DLL and its configuration entry. The shared loader and other mods are kept.
Backups and logs are also kept; you may remove them manually. For a manual uninstall, remove
`position.dll` and only its entry from `Mods=`.

## Troubleshooting

- Only the verified **Steam 1.55** binaries are supported; other versions are rejected.
- Check `position.log` next to the installed DLL. It records initialization, the first coordinate
  sample, the first DirectX draw and F8 visibility changes.
- If the overlay does not appear, verify that `Mods=` still contains `position.dll` and that the full
  release was extracted. Compatibility with other graphics overlays is not exhaustively tested.
- The mod uses Windows' Direct3D 11 and shader compiler. It is intended for Windows 10/11 x64.

## Build and tests

Requires Visual Studio Build Tools with **MSVC v141 14.16**, the Windows SDK and PowerShell.
The C++ runtime is linked statically. Building does **not** require any game binaries.

```powershell
.\build.ps1
.\tests\run.ps1                         # Real Direct3D WARP pixel test; no game needed
.\tests\run.ps1 -GameDir '<game folder>' # Also tests read-only copies of the supported DLLs
.\package.ps1
```

The loader's checked-in export declarations forward the standard Windows multimedia APIs to the
system `winmm.dll`; they do not contain Windows or game binaries. To regenerate them, use Python
with `pefile`: `python loader/gen_def.py C:\Windows\System32\winmm.dll`.

Player/camera reads use verified offsets and functions in the supported build. Only copied vectors
cross from the camera update thread to the render thread. The DirectX drawing code saves and restores
the graphics state it changes and releases temporary backbuffer references after each frame.

The DirectX overlay and F8 toggle were confirmed working in a game session. Automated checks cover
coordinate sampling, missing/invalid data, version rejection, toggling, real rendered text pixels,
hidden-panel pixels and restoration of selected graphics states. Resolution changes and combinations
with other graphics mods have not been exhaustively tested.

This is an unofficial mod. Dying Light belongs to Techland. No game binaries, game assets, game saves
or proprietary decompilation exports are included in this repository or its releases.
