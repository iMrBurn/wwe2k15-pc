# WWE 2K15 PC on Linux / Steam Deck (Proton) — guide and status

**Status: not tested on Linux yet.** Nobody on the team has a Linux machine; this page is a code review plus the numbers measured on
a Windows PC that stands in for a Steam Deck, and a checklist for the first player who tries it. Reports are welcome (log files from
`logs/`, the Proton version, the GPU).

You need your own copy of the game (and Title Update 4); nothing here links to game files.

## How to run it (expected to work)

1. Install the port (the zip) in a folder the Proton prefix can reach, for example `~/Games/WWE2K15-PC/`, and put your game folder as
   `game/` beside the exe (see the README).
2. Steam → *Add a Non-Steam Game* → pick `wwe2k15_native.exe` (the native renderer; the Xenos `wwe2k15.exe` needs the fragment-shader-interlock
   extension that RADV does not expose, it falls back to a slower path on AMD) → Properties → Compatibility → *Proton Experimental*
   (or a recent Proton-GE). The launcher (`WWE 2K15 Launcher.exe`) also uses Direct3D 11 / WinHTTP, both fine under DXVK/Wine, but starting
   the exe directly is simpler and what the Deck needs.
3. Launch options (optional): `--fullscreen=true --svr_render_scale=720` on a Deck (1280x800 screen); lower (`540`, `480`) if a weaker device drops frames.
4. First start only: the game converts its shaders (about 600) in the background; the picture fills in over the first seconds
   (about 16 s on one CPU thread) and the very first match may hitch while the driver builds its pipelines. Both caches are saved in
   `cache/` (shaders) and the user data folder (`pipelines.bin`) and reused afterwards. Do not copy a cache from another PC.

## Numbers measured on a stand-in (Windows 11, AMD Radeon iGPU, 8 logical processors, window 1280x800, render height 720, audio muted)

| Case | fps | worst frame gap | CPU (share of one core) | Memory (working set / private) |
|---|---|---|---|---|
| 1v1 demo match | 60.0 | 18 ms | 72 % | 1.13 GB / 1.71 GB |
| Elimination Chamber 6-man | 59.5-59.8 | 32-38 ms | 111 % | peak 1.40 GB / 1.65 GB |
| Entrances at 60 fps (1v1, scenes of 81 s) | 58-60 (no fallback needed) | | | peak 1.25 GB / 1.53 GB |

The Steam Deck's GPU (RDNA2, 8 CU) is stronger than this iGPU; its CPU (Zen 2, 4 cores / 8 threads, lower clocks than a desktop) is the
unknown. Entrances and victory scenes run at 60 fps by default (`--wwe2k15_60fps=false` gives the console's 30); if a scene cannot hold 60 the
game gives it up by itself for the rest of the session (`wwe2k15_60fps_auto`, log line "[fps] scene cannot hold 60 fps").

## What may break under Proton / Wine (code review of the port)

| Area | What the code does | Risk and what to check |
|---|---|---|
| Shader conversion | `dxcompiler.dll` (Windows DLL beside the exe) loaded with `LoadLibraryW`, called through COM (`DxcCreateInstance`), runs on the CPU only | Should load under Wine like any DLL. If it fails the log says "dxcompiler.dll missing or unusable ... most of the picture stays black". First thing to check in a bug report. |
| GPU | Vulkan 1.2 through plume; needs buffer device address and descriptor indexing | Mesa RADV on the Deck has both. With several GPUs `--svr_gpu_device="<name>"` picks one; the Deck has one. |
| Window / fullscreen | SDL window, `--fullscreen`, `--monitor`, `--window_width/height` | Fine under gamescope; check that the 16:9 image is letterboxed on the 16:10 screen. |
| Controller | SDL game controller + `gamecontrollerdb.txt` | Steam Input presents a virtual Xbox pad: expected to work. The keyboard fallback (`--mnk_mode`) exists. |
| Audio | SDL audio; if no device can be opened the game now runs silent instead of failing (patch `rexglue-sdk-silent-audio`) | Check the log line "audio endpoint". |
| Timers / priorities | `timeBeginPeriod(1)`, `SetThreadPriority` (below normal for shader and pipeline threads) | Harmless if ignored by Wine. No process or thread affinity is set by the game. |
| Paths | User data `Documents\WWE2K15` (from `SHGetKnownFolderPath`), fallbacks `%LOCALAPPDATA%\WWE2K15` then `user_data/` beside the exe; shader cache `cache/` beside the exe; game data from `--game_data_root` or `game/` | Inside Proton `Documents` is in the prefix (`drive_c/users/steamuser/`). Paths with spaces are fine; non-ASCII characters in the install path are the main untested risk (some code converts paths to narrow strings). Prefer an ASCII path. A `Z:\home\deck\...` path works. |
| NVIDIA hint | `NvOptimusEnablement` exported from the exe | Harmless on AMD / Wine. |
| Crash reports | `crash_*.txt` + minidump through dbghelp | May be partly empty under Wine; the log file is the useful one. |
| Memory | about 1.1-1.4 GB working set, 1.7 GB private on the stand-in | A 4 GB machine is enough; a Deck has 16 GB shared with the GPU (the log line "vram X / Y MB" shows the budget the driver gives). |

## Reporting a problem from Linux

Send `logs/wwe2k15_*.log` (and `crash_*.txt` if present), the Proton version (or Wine + DXVK/VKD3D versions), the distribution, and the GPU
(`vulkaninfo --summary`). The first lines of the log name the Vulkan devices ("Vulkan GPU: ..."), the chosen one ("GPU caps: ...") and the
shader conversion progress ("Native shaders: ...").
