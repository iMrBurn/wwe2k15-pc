# WWE 2K15 PC

The project goal is a Windows PC port of the Xbox 360 release, following the approach used by Josiah's SmackDown vs. Raw ports and this user's WWE '12 P project: statically recompile the game's PowerPC code with ReXGlue, then replace the Xbox Xenos GPU drawing path with a title-specific native Windows renderer. “Native” here means native host rendering; ReXGlue still provides the recompiled game code and required Xbox system services.

This repository contains project code and documentation only. It does not contain game data. Use files dumped from your own copy of the Xbox 360 game, and keep executables, disc images, extracted assets, generated code, and build outputs out of Git.

## Status

**0.1.0 beta is out**: download it from the [releases page](https://github.com/iMrBurn/wwe2k15-pc/releases). It runs the whole game with Title Update 4 and DLC at 60 fps in matches on the Xenos GPU plugin (Vulkan), with a launcher in five languages, automatic updates, save backups, a mods folder and crash/freeze reports. No game data is included: you need your own copy of the Xbox 360 game and its Title Update 4. See the release notes in [docs/releases](docs/releases) and the work log in [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md).

Requirements: 64-bit Windows 10/11, a 64-bit CPU from 2009 or newer (4 threads or more recommended), a Vulkan GPU. Tested so far on NVIDIA; AMD and Intel GPU reports are welcome.

### Linux and Steam Deck

There is no native Linux build yet, but the Windows build should run through Proton: add `WWE 2K15 Launcher.exe` to Steam as a non-Steam game, force a Proton version in its properties (Compatibility), and start it. Reports from Linux and Steam Deck players are welcome (open an issue, working or not). A native build may follow if there is demand.

### Building

Launch `Lancer WWE 2K15.bat` or `WWE 2K15 Launcher.exe` beside `wwe2k15.exe`. Build with `port\tools\build.ps1` (`-Sdk` also builds the patched ReXGlue SDK) and package with `port\tools\package.ps1`. The launcher uses Dear ImGui from the ReXGlue SDK; when building against an installed SDK, set `WWE2K15_IMGUI_DIR` to that SDK source tree's `thirdparty/imgui` folder.

The Xbox executable and extracted assets stay outside Git. Before reusing any code from another project, preserve its license and notices and verify compatibility with this repository's license.

## Asset research

Run the metadata-only scanner against your own extracted game folder:

```powershell
python native/tools/inspect_game_data.py "C:\path\to\WWE2K15" --samples 1
```

It reports file counts, sizes, recognized signatures, and bounded PAC index candidates. It does not extract or modify game data.

## Disclaimer

This is an unofficial fan project and is not affiliated with WWE, 2K, or Yuke's. All game names and related marks belong to their respective owners.
