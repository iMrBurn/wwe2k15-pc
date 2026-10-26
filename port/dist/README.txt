WWE 2K15 PC — version {VERSION} (beta)
=====================================

Unofficial PC port of WWE 2K15 (Xbox 360), statically recompiled with ReXGlue.
No game data is included: you need your own copy of the Xbox 360 game and its
Title Update 4.

REQUIREMENTS
- 64-bit Windows 10/11, a 64-bit CPU from 2009 or newer (SSE4.2), 4 threads or more
  recommended.
- Vulkan GPU. Recommended: recent NVIDIA or Intel (accurate "FSI" rendering).
  AMD GPUs run without FSI and may show black screens during entrances.

SETUP
1. Copy your Xbox 360 game files (default.xex, pac, sound, movies...) into the
   "game" folder next to the launcher, or keep them elsewhere and pick the folder
   in the launcher (SETTINGS > BROWSE).
2. Start "WWE 2K15 Launcher.exe".
3. SETTINGS > ADD TU / DLC...: pick the Title Update 4 file (tu00000004_00000000)
   and, if you own them, your DLC packages (folder 545408B6\00000002).
4. Choose your language in SETTINGS, then click PLAY.

The first start takes longer (DLC install, shader preparation).

UPDATES
The launcher checks GitHub for new versions at startup (ABOUT page). Installing an
update keeps your game folder, mods, logs, settings and saves.

FOLDERS
- game\                    : your game files (not included).
- logs\                    : game logs, crash_*.txt / freeze_*.txt reports.
- mods\                    : files that replace the game's own, at the same path
                             (for example mods\pac\menu\menu.pac).
- cache\                   : shader cache (safe to delete).
- %LOCALAPPDATA%\WWE2K15\  : saves, save backups and installed DLC.

REPORTING BUGS
Give your GPU, the scene (menu, entrance, match...) and attach the latest
logs\wwe2k15_*.log (and the crash_ / freeze_ report if the launcher shows one) and
a screenshot if you can.

Never share the game files, the Title Update or DLC.
Not affiliated with WWE, 2K or Yuke's.
