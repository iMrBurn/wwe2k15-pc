WWE 2K15 PC - version {VERSION} (beta)
======================================

Unofficial PC port of WWE 2K15 (Xbox 360). No game data is included: you need YOUR OWN
copy of the Xbox 360 game (extracted files or an .iso) AND the Title Update 4 (TU4).
Without both, the game will not start. Do not ask anyone for the files.


1. WHAT YOU NEED
- Windows 10 or 11, 64-bit, a CPU from 2009 or newer (4 threads or more recommended).
- A graphics card with Vulkan (almost every GPU from the last ~8 years; update your driver!).
  The "native renderer" needs Vulkan 1.2.
- Your WWE 2K15 Xbox 360 game: the folder that contains "default.xex" (and "pac", "sound",
  "movies"...), or the .iso file.
- The Title Update 4 package, named "tu00000004_00000000" (no extension). DLC are optional.


2. FIRST START, STEP BY STEP
1) Unzip the whole archive to a normal folder (for example C:\WWE2K15). NOT inside the zip,
   NOT in "Program Files", NOT on the Desktop of a synced OneDrive.
2) Put your game files in the "game" folder next to the launcher (so that
   game\default.xex exists), or keep them where they are and use SETTINGS > BROWSE.
   For an .iso: SETTINGS > BROWSE and pick the .iso, or SETTINGS > CHOOSE AN ISO.
3) Start "WWE 2K15 Launcher.exe" (not wwe2k15.exe).
4) SETTINGS > ADD TU / DLC... and pick the Title Update 4 file (tu00000004_00000000).
   Do the same for your DLC if you have them.
5) On the PLAY page, "Game" and "Title Update" must both be green. If a red note is shown,
   read it: it tells you what is missing.
6) Pick your language in SETTINGS, then click PLAY.
The first start is longer (installing the DLC, preparing shaders): wait, it is normal.


3. GRAPHICS: WHAT TO TICK
- Normal PC: leave the defaults (720p is a good start; raise it if your PC is strong).
- Weak PC, laptop, integrated graphics (Intel UHD, AMD Vega...): tick
  GRAPHICS > "Rendu natif (expérimental)". On an AMD integrated GPU it goes from 15-40 fps
  to a steady 60. Keep the resolution on AUTO or 720p/540p.
- If the native renderer does something wrong (black screen, glitch, crash), untick it
  and use the normal renderer. The normal renderer on AMD cards picks a lighter mode by itself.
- PERFORMANCE preset = lowest cost for the normal renderer.
- Keyboard: you can type names in the game's text screens with your keyboard. To play,
  a controller is recommended (Xbox pad); the keyboard works too (see the Controls in SETTINGS).


4. IT DOES NOT START? CHECK THIS FIRST
- The launcher shows a red note on the PLAY page: follow it (usually TU4 missing).
- Message "Failed to load XEX ... C0000001/C0000002": your Title Update 4 is missing or is not
  the one for your copy of the game, or the game files are incomplete/corrupted.
- The game opens and closes at once: update your graphics driver, then try the native
  renderer (or untick it). Send us logs\wwe2k15_*.log and the crash_*.txt.
- Nothing happens when you click PLAY: antivirus may block it; add the folder as an exception.
- Do NOT start the game from inside the zip, and do not run two copies at the same time.


5. UPDATES
The launcher checks GitHub for new versions (ABOUT page) and installs them for you. An update
keeps your game folder, mods, logs, settings and saves.


6. FOLDERS
- game\                    : your game files (not included).
- game_update\             : title update and DLC when you play from an .iso.
- logs\                    : game logs and crash_*.txt / freeze_*.txt reports.
- mods\                    : files that replace the game's own at the same path
                             (for example mods\pac\menu\menu.pac). See docs/MODS.md on GitHub.
- cache\                   : shader caches (safe to delete, they are rebuilt).
- %LOCALAPPDATA%\WWE2K15\  : saves, save backups and installed DLC.


7. LINUX / STEAM DECK
No native Linux build. Add "WWE 2K15 Launcher.exe" to Steam as a non-Steam game, force
Proton in its properties (Compatibility) and start it. Please tell us how it goes.


8. REPORTING A BUG (so that we can fix it)
Open an issue: https://github.com/iMrBurn/wwe2k15-pc/issues and give:
- your graphics card and driver version, your CPU and RAM;
- which renderer (normal or native) and your resolution;
- what you were doing (menu, entrance, match, mode...);
- the newest file in the "logs" folder (wwe2k15_*.log), and the crash_*.txt if there is one;
- a screenshot if it is a visual problem.
"It does not work" without a log cannot be fixed.

Never share the game files, the Title Update or the DLC.
This project is GPL-3.0 (see the licenses folder). Not affiliated with WWE, 2K or Yuke's.
