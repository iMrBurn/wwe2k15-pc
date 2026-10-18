# WWE 2K15 PC — beta test notes

## What this build is

This is an early Windows prototype built from the Xbox 360 game's own executable using ReXGlue, with the Xenos graphics plugin and Vulkan. It is not a finished native-renderer port. The game code is generated from each tester's own `default.xex`; the executable, assets, videos, and audio are not distributed by this repository.

## Current status

- Confirmed on the maintainer's RTX 5070 system: main menu, entrances, and a complete 1v1 match; gameplay has reached about 60 FPS.
- Vulkan is the selected graphics backend. D3D12 hung on this title in earlier tests.
- The latest complete-match log ends with a normal window-close and title shutdown, with no crash reported.
- The log contains thousands of repeated allocator probe misses during startup. These are being demoted from error-level logging because they are expected candidate-range probes; allocation behavior is unchanged. XCTD compression-info queries also repeat, and their log severity is reduced while the unsupported result remains unchanged.
- Known visual/performance issues still need in-game A/B confirmation: intermittent match flicker, corrupted arena LED/tron textures, slow pre-match videos, and long initial menu loading. The fuzzy-alpha setting is experimental.
- Expect crashes, missing content, and machine-specific performance. Keep saves and game data backed up.

For renderer research only, the project also has an optional D3D call census build (`-DWWE2K15_D3D_CENSUS=ON`). It writes `wwe2k15_d3d_census.csv` while running and may reduce performance; do not use it for normal play or FPS comparisons.

## Run a local test build

1. Use your own legally obtained Xbox 360 game dump and keep it on your machine.
2. Run `Lancer WWE 2K15.bat`, browse to the game folder containing `default.xex`, and click **LANCER WWE 2K15**. The launcher remembers the folder, selects Vulkan and SDL input, and stores user data under `%LOCALAPPDATA%\WWE2K15`.
3. The first launch can take a while while the menu and shaders initialize. Later launches should reuse the local cache.

The launcher includes a button for opening the build's log folder. The Vulkan log showed that OBS's global Vulkan layer was injected even in the previous no-OBS attempt, so that launcher option is not treated as a confirmed fix.

Do not upload `default.xex`, extracted game files, generated recompilation output, or the resulting game executable. Share logs with personal paths and account names removed.

## Useful reports

For each report, include GPU and driver, Windows version, scene, whether it was the first launch, approximate FPS, and the newest `wwe2k15_*.log` from the build's `logs` folder. For visual defects, include a screenshot or short capture and say whether the issue appears in menus, entrances, or the match itself. Change one setting per comparison and preserve the prior config.
