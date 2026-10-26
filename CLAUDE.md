# WWE 2K15 PC — instructions for Claude

Native Windows x64 port of WWE 2K15 (Xbox 360) by static recompilation with ReXGlue, rendered
through the Xenos plugin on Vulkan. Main notes: `docs/PORTING_PLAN.md` (dated findings, newest at
the bottom), `docs/BETA_TESTING.md` (tester-facing).

The user (iMrBurn) is French-speaking and not a developer: answer in French, simply, no jargon.
Code, commits and docs in English.

## Hard rules
- **No game data in Git or in any package**: no xex/xexp, iso, .pac, .bk2, sound, textures,
  `port/generated/`, dumps, Title Update or DLC, no download links for them.
- **Nothing is pushed to GitHub without the user's explicit go.** The user pushes or says "push".
- Ask before any download. Never touch Windows security settings.
- Never modify the user's original game folders (`Desktop\WWE2K15`, `Desktop\dlc 2k15`,
  `Desktop\tu00000004_00000000`). Work in `C:\Dev\games\WWE2K15-TU4` (hard links + TU + DLC).
- The user has said Claude may launch the game on this PC while they are away. Only one game
  instance at a time; close it before rebuilding.

## Layout and build
- Repo: `C:\Dev\wwe2k15-pc2` (branch `tu4` = TU4 + DLC build). The older `C:\Dev\wwe2k15-pc`
  checkout cannot be written by Claude's git; do not use it.
- SDK: `C:\Dev\rexglue-sdk` at c94f5eb + `port/patches/*.patch` (NOT `xctd-compression-info`),
  installed to `out\install\ninja-clangcl`. After an SDK change: rebuild/install, regenerate the
  matching patch in `port/patches/` with `git diff` (LF, full context) and check that every patch
  still applies on a pristine c94f5eb worktree.
- Build: `port\tools\build.ps1` (`-Sdk` to rebuild the SDK first). It also copies the SDK DLLs and
  `wwe2k15_runtime.toml` next to the exe (a plain CMake build does neither).
- Codegen input: `port/wwe2k15_manifest.toml` (local, ignored) points at the TU4 game folder; the
  code is generated from the TU-patched image (`default.xexp` beside `default.xex`).
- Logs: `port\out\build\win-amd64-release\logs`. Per-category debug levels go in the exe-side
  `wwe2k15.toml` under `[log.levels]` (e.g. `krnl = "debug"`, `fs = "debug"`).

## Testing without the user
- Keyboard as pad: `--mnk_mode=true`, keys must be held ~250 ms. Plain arrows rebound to the D-pad
  with `--keybind_dpad_up=Up` etc. A = Space, Start = Enter, Y (random) = P, left stick = WASD.
- Scratchpad helpers used so far: `nav.ps1` (key sequences + screenshot), `shotloop.ps1`
  (screenshots + per-thread CPU every N s), `fullrun.sh` (launch → 1v1 Cena vs Reigns → capture
  entrances). A fully black capture is a ~3 KB PNG.
- The Vulkan log prints every 5 s: swap requests, async-shader skips and (SDK diagnostic) draws per
  frame / resolves. 300 swaps = 60 fps, 150 = 30 fps.

## Known facts
- Function hints and hooks are TU4 addresses; comments keep the base-game address.
- `gpu_allow_invalid_fetch_constants` must stay false (stale textures); GPU-written targets are
  accepted by the SDK patch instead.
- `render_target_path_vulkan = "fsi"` fixes the entrance black frames (host-RT EDRAM path loses the
  frame). AMD has no fragment shader interlock and falls back to host RT.
- `streaming_install.pck` and per-DLC sound banks are requested but absent; not fatal.
- `readback_resolve=fast` crashed Vulkan earlier; `full` works but did not fix black frames.
