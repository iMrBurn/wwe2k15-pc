# WWE 2K15 PC — instructions for Claude

Native Windows x64 port of WWE 2K15 (Xbox 360) by static recompilation with ReXGlue, rendered
through the Xenos plugin on Vulkan. Public repo: https://github.com/iMrBurn/wwe2k15-pc. Notes:
`docs/PORTING_PLAN.md` (dated findings, newest at the bottom), `docs/releases/` (release notes),
`docs/BETA_TESTING.md` (tester-facing). Sister project: the user's WWE '12 PC port
(https://github.com/iMrBurn/wwe12-pc), whose CLAUDE.md and fixes are often worth reading first
(it uses its own native renderer, not the Xenos plugin, so fixes do not always transfer as is).

The user (iMrBurn) is French-speaking and not a developer: answer in French, simply, no jargon.
Code, commits and docs in English.

## Hard rules
- **No game data in Git or in any package**: no xex/xexp, iso, .pac, .bk2, sound, textures,
  `port/generated/`, dumps, Title Update or DLC, no download links for them.
- GitHub: Claude commits and pushes itself from `C:\Dev\wwe2k15-pc2` (branch `tu4`; `main` gets
  merges from `tu4`). Tags `v<VERSION>` are pushed by Claude. **Releases (page + zip) are created by
  the user on the website**; never use the GitHub API with stored credentials. End commit messages
  with the Co-Authored-By line given by the harness.
- Ask before any download. Never touch Windows security settings.
- Never modify the user's original game folders (`Desktop\WWE2K15`, `Desktop\dlc 2k15`,
  `Desktop\tu00000004_00000000`). Work in `C:\Dev\games\WWE2K15-TU4` (hard links + TU + DLC).
- Ask before taking the PC (launching the game, sending keys) when the user may be using it. Only
  one game instance at a time; close it before rebuilding (the link fails while the exe or the
  launcher is running).

## Conversations (several chats work on 2K15 at once)
Each conversation has a topic and stays in it; it notes its findings in `docs/PORTING_PLAN.md`
(dated section) and ticks its line in "Open work" below when done and tested.
- **(Main)**: coordination, versions and releases, GitHub, testers' reports, merging `tu4` into
  `main`. Only Main bumps `VERSION`, packages and tags.
- **(Rendu)**: graphics bugs (AMD / Intel, black frames, textures), Vulkan / SDK GPU code.
- **(Perf)**: CPU/GPU optimisation, loading times, low-end PCs (540p research).
- **(Launcher)**: launcher UI, options, updater, translations.
- **(Mods)**: mods folder, Superstar Threads / creations, texture packs.
All chats share one checkout and one SDK: before building or launching the game, check that no
other build (`ninja`, `clang-cl`) or `wwe2k15.exe` is running (`tasklist`); commit only your own
files, with `git add <paths>`, never `git add -A`; never overwrite another chat's uncommitted work.

## Layout and build
- Repo: `C:\Dev\wwe2k15-pc2`. The older `C:\Dev\wwe2k15-pc` checkout cannot be written by
  Claude's git; do not use it.
- SDK: `C:\Dev\rexglue-sdk` at c94f5eb + `port/patches/*.patch` (NOT `xctd-compression-info`),
  installed to `out\install\ninja-clangcl`. It builds with PDBs (`/Z7`, `/DEBUG /OPT:REF /OPT:ICF`;
  PDBs in `out\win-amd64\Release`). After an SDK change: rebuild/install, regenerate the matching
  patch in `port/patches/` with `git diff` against a worktree of c94f5eb with the other patches
  applied and committed (LF), and check that every patch still applies there.
  `src/graphics/vulkan/command_processor.cpp` also holds local diagnostic counters (`g_frame_*`,
  "Frame activity" log) that are in no patch: leave their hunks out of patches.
- Build: `port\tools\build.ps1` (`-Sdk` to rebuild the SDK first). It also copies the SDK DLLs and
  `wwe2k15_runtime.toml` (as `wwe2k15.toml`) next to the exe. After a codegen change the first
  build may fail with "precompiled header ... has been modified": run it again.
- Package: `port\tools\package.ps1` -> `dist\WWE2K15-PC-v<VERSION>.zip` (entries with `/`, top
  folder `WWE2K15-PC-v<VERSION>/`), `dist\SHA256-v<VERSION>.txt`, `dist\symbols-v<VERSION>\`.
- Codegen input: `port/wwe2k15_manifest.toml` (local, ignored) points at the TU4 game folder; the
  code is generated from the TU-patched image. Codegen register options (`skip_lr`, `*_as_local`)
  were tried on 2026-10-26: no speed gain, `non_argument_as_local` hangs at boot; leave them off.
- Logs: `port\out\build\win-amd64-release\logs`. Per-category levels go in the exe-side
  `wwe2k15.toml` under `[log.levels]`. Every 5 s: "Guest pacing" (fps, worst frame gap) and the
  Vulkan line (300 swaps = 60 fps). Crash/freeze reports: `logs\crash_*.txt`, `logs\freeze_*.txt`.

## Release procedure (Main)
1. `VERSION` (for example `0.1.2-beta`), build with the game and launcher closed, check that the
   launcher exe contains the new version string, `package.ps1`.
2. `docs/releases/v<VERSION>.md` (English + French, SHA-256 line at the end).
3. Commit, push `tu4`, merge into `main` (`--no-ff`), tag `v<VERSION>`, push `main` and the tag.
4. The user creates the GitHub release from the tag, pastes the notes, attaches the zip with its
   exact name, ticks "pre-release". The launcher updater (`port/src/launcher/updater.h`) only takes
   a release with a `.zip` asset from this repo; an update keeps `game/`, `mods/`, `logs/` and the
   launcher settings, and replaces `wwe2k15.toml`.

## Testing (port/tools/dev, outputs in work\dev, ignored by Git)
- Keyboard as pad: `--mnk_mode=true`; A = Space, Start = Enter, random = P, left stick = W A S D.
  The user's keyboard is AZERTY: send Z for the stick up. Short holds (120-150 ms) move menus one
  step.
- `nav.ps1 -keys "..." -shot name` (keys only reach the game window, then a capture),
  `thread_cpu.ps1` (per-thread CPU %), `sampler.ps1` (RIP samples of the busiest threads;
  symbolize `wwe2k15.exe` offsets with `llvm-symbolizer --obj wwe2k15.exe` at 0x140000000+offset,
  SDK DLL offsets at 0x180000000+offset with the PDBs), `stackscan.ps1` (return addresses on every
  thread's stack, for hangs), `ab3.sh <tag> <exe dir> [args]` (title -> 1v1 match, CPU at +150 s),
  `demo_measure.sh <tag> <exe dir>` (waits for the attract-mode demo match, which replays the last
  match played, and measures it without any key press).
- Main menu path to creations: Exhibition is first; CREATIONS is 4 steps down; PANOPLIE
  SUPERSTARS (Superstar Threads) is the 2nd entry of its submenu.

## Known facts
- Function hints and hooks are TU4 addresses; comments keep the base-game address.
- `gpu_allow_invalid_fetch_constants` must stay false (stale textures).
- `render_target_path_vulkan = "fsi"` fixed the entrance black frames (Oct 20-22). Retested on
  2026-10-26: the host RT path (`rtv`, used by GPUs without fragment shader interlock) no longer
  loses entrances on NVIDIA nor on the PC's AMD iGPU (`--vulkan_device=1`, which also has FSI); only
  short black frames at loading transitions. If an AMD tester still reports it: ask for the log and
  the GPU model.
- Superstar Threads / created attire: the game composes textures on the GPU and compresses them on
  the CPU; `readback_resolve = "full"` with `readback_resolve_formats = "2,6,15"` and
  `readback_resolve_repeat_limit = 60` (SDK patch `rexglue-sdk-readback-resolve.patch`) reads them
  back without costing match fps. 16:9 targets of 1024 px and more are skipped by shape.
- Presentation uses mailbox (`vulkan_allow_present_mode_immediate = false`): immediate tore.
- The render thread's GPU-ring wait yields (`wwe2k15_render_wait_yield`, about -45 % CPU).
- `text_hook.cpp` turns the game's "Xbox 360 console" warnings into "PC" (5 languages).
- `streaming_install.pck` and per-DLC sound banks are requested but absent; not fatal.
- 6-man Elimination Chamber holds 60 fps on the user's PC (RTX 5070 Ti) since 0.1.1; the game
  threads wait 88-92 % of the time there.

## Open work
- (Main) Testers' reports on AMD / Intel GPUs, older CPUs, controllers, Linux/Proton.
- (Perf) Faster loading: WWE '12 raised its loader's time budget per frame (`sub_82819258`, x2);
  2K15's equivalent is not found yet (sample a real loading screen).
- (Perf) Issue #4 (Vega 7 iGPU, 30-37 fps, 4.7 s load stall): persistent driver pipeline cache added
  (`rexglue-sdk-pipeline-cache.patch`), its gain on the tester's old driver is unconfirmed; the iGPU is GPU-bound
  (see PORTING_PLAN 2026-10-27).
- (Perf) True 540p (and lower) for weak GPUs: the Xenos plugin only scales by whole multiples of
  720p; would need the game to render smaller.
- ~~(Rendu) AMD entrance black frames without FSI~~: not reproduced on 2026-10-26 (see Known facts).
- ~~(Rendu) Log which render target path the SDK picked~~: done (`rexglue-sdk-render-path-log.patch`,
  one "render target path: ..." gpu info line at startup). In the next release.
- (Launcher) Possibly a square icon variant for small sizes.
