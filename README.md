# WWE 2K15 PC

The project goal is a Windows PC port of the Xbox 360 release, following the approach used by Josiah's SmackDown vs. Raw ports and this user's WWE '12 P project: statically recompile the game's PowerPC code with ReXGlue, then replace the Xbox Xenos GPU drawing path with a title-specific native Windows renderer. “Native” here means native host rendering; ReXGlue still provides the recompiled game code and required Xbox system services.

This repository contains project code and documentation only. It does not contain game data. Use files dumped from your own copy of the Xbox 360 game, and keep executables, disc images, extracted assets, generated code, and build outputs out of Git.

## Status

`port/` contains the ReXGlue prototype. The next major step is a native renderer that hooks the game's XDK Direct3D calls, maps its resources and draw state to a Windows graphics API, and uses shaders extracted and converted from the game. The Xenos GPU plugin can remain as a development fallback while native coverage grows; it should not be the final draw path. See [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md) for the captured baseline and [docs/NATIVE_PORT_PLAN.md](docs/NATIVE_PORT_PLAN.md) for the renderer plan.

The prototype has reached an early local beta-test milestone: the maintainer has confirmed a complete 1v1 match at about 60 FPS on an RTX 5070. Flicker, LED/tron texture corruption, slow pre-match videos, and long first-menu loading remain unresolved. This is not a general release; see [docs/BETA_TESTING.md](docs/BETA_TESTING.md) for status and test guidance. Builds must be generated from the tester's own game executable and must not be redistributed with game code or assets.

For a local build, launch `Lancer WWE 2K15.bat` beside `wwe2k15.exe`. The native Windows launcher has Play, Settings, Controls, Mods, Saves, and About pages, remembers the folder containing your own `default.xex`, and opens the build logs. It starts the Xenos plugin with Vulkan and SDL controller input. Its interface uses Dear ImGui from the ReXGlue SDK; when building against an installed SDK, set `WWE2K15_IMGUI_DIR` to that SDK source tree's `thirdparty/imgui` folder.

The Xbox executable and extracted assets stay outside Git. This repository will document the game-specific call mappings, shader pipeline, renderer hooks, and fixes needed to adapt the ReXGlue/re:Blue/plume architecture to WWE 2K15. Before reusing any code from another project, preserve its license and notices and verify compatibility with this repository's license.

## Asset research

Run the metadata-only scanner against your own extracted game folder:

```powershell
python native/tools/inspect_game_data.py "C:\path\to\WWE2K15" --samples 1
```

It reports file counts, sizes, recognized signatures, and bounded PAC index candidates. It does not extract or modify game data.

## Disclaimer

This is an unofficial fan project and is not affiliated with WWE, 2K, or Yuke's. All game names and related marks belong to their respective owners.
