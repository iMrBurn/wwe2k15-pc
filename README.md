# WWE 2K15 PC

Unofficial Windows PC port of **WWE 2K15 (Xbox 360)**, made by statically recompiling the game's PowerPC code with [ReXGlue](https://github.com/rexglue/rexglue-sdk). Two renderers are included: the Xbox GPU emulation (Vulkan) and an **experimental native renderer** (Vulkan, based on the work of the WWE '12 PC port, re:Blue and plume) that makes weak PCs and integrated GPUs run the game at 60 fps.

**This repository contains project code and documentation only. No game data is included or linked.** You need your own copy of the Xbox 360 game and its Title Update 4. Please do not ask for game files in the issues.

> **Status: beta, best effort.** This is a hobby project maintained by one person next to other projects. Bug reports are read and fixed in batches: a fix can take a few days. The more complete your report (GPU, log, steps), the faster it gets fixed.

## Download

Get the latest release (a `.zip`) from the **[releases page](https://github.com/iMrBurn/wwe2k15-pc/releases)**. The launcher updates itself afterwards (ABOUT page). Release notes are in [docs/releases](docs/releases).

## Quick start

1. **Unzip** the archive into a normal folder (not in the zip, not in `Program Files`).
2. Put **your** game files in the `game` folder (so that `game\default.xex` exists), or use `SETTINGS > BROWSE` (a `default.xex`, or an `.iso`).
3. Start **`WWE 2K15 Launcher.exe`** (not `wwe2k15.exe`).
4. `SETTINGS > ADD TU / DLC...`: pick the **Title Update 4** package (`tu00000004_00000000`). Add your DLC if you have them.
5. On the `PLAY` page, *Game* and *Title Update* must be green (a red note tells you what is missing), then click `PLAY`. The first start takes longer: that is normal.

**Weak PC, laptop or integrated GPU?** Tick `GRAPHICS > "Rendu natif (expérimental)"`. On an AMD integrated GPU a match goes from 15-40 fps to a steady 60. If it misbehaves, untick it.

### It does not start?

- A red note on the `PLAY` page: follow it (usually the Title Update 4 is missing).
- `Failed to load XEX ... C0000001`: the Title Update 4 is missing or does not match your copy of the game, or the game files are incomplete.
- The game closes at once: update your graphics driver and try the native renderer (or untick it).
- Read the full guide in [`port/dist/README.txt`](port/dist/README.txt) ([français](port/dist/LISEZMOI.txt)).

## Requirements

64-bit Windows 10/11, a 64-bit CPU from 2009 or newer (4 threads or more recommended), a Vulkan GPU (the native renderer needs Vulkan 1.2; update your driver). Tested so far on NVIDIA, AMD (integrated and RX 6000) and one Intel report in progress: GPU reports are welcome.

## Features

- The whole game with Title Update 4 and DLC, 60 fps in matches.
- Launcher in five languages, with automatic updates from GitHub, save backups, a mods folder ([docs/MODS.md](docs/MODS.md)), ISO support, crash and freeze reports.
- Keyboard text entry in the game's text screens, resolution choice (540p to 4K), FPS counter, replay.
- "Xbox 360" texts shown as "PC".

### Linux and Steam Deck

No native Linux build yet; the Windows build should run through Proton: add `WWE 2K15 Launcher.exe` to Steam as a non-Steam game, force a Proton version in its properties (Compatibility) and start it. Reports are welcome (open an issue, working or not).

## Reporting a bug

[Open an issue](https://github.com/iMrBurn/wwe2k15-pc/issues) with: your graphics card and driver, CPU and RAM, which renderer (normal or native), what you were doing, the newest `logs\wwe2k15_*.log` (and the `crash_*.txt` if there is one) and a screenshot for visual problems. Reports without a log are very hard to fix. Never share the game files, the Title Update or the DLC.

## For developers

Work log and technical notes: [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md), native renderer: [docs/NATIVE_PORT_PLAN.md](docs/NATIVE_PORT_PLAN.md), contributing rules for the multi-chat workflow: [CLAUDE.md](CLAUDE.md).

- Build with `port\tools\build.ps1` (`-Sdk` also builds the patched ReXGlue SDK; the SDK patches live in `port/patches`). The native renderer needs `port\tools\setup_native_deps.ps1` (plume, XenosRecomp) and the CMake option `WWE2K15_NATIVE_RENDERER=ON`. Package with `port\tools\package.ps1` (`-Native` includes the native renderer).
- The launcher uses Dear ImGui from the ReXGlue SDK; when building against an installed SDK, set `WWE2K15_IMGUI_DIR` to that SDK source tree's `thirdparty/imgui` folder.
- The Xbox executable, extracted assets and everything derived from them (generated code, shader caches) stay outside Git and outside the packages. Before reusing code from another project, preserve its license and notices.
- Metadata-only scanner for your own extracted game folder: `python native/tools/inspect_game_data.py "C:\path\to\WWE2K15" --samples 1`.

## License

[GPL-3.0](LICENSE). Third-party components and their licenses: [THIRD_PARTY.md](THIRD_PARTY.md).

## Disclaimer

This is an unofficial fan project and is not affiliated with WWE, 2K, or Yuke's. All game names and related marks belong to their respective owners.
