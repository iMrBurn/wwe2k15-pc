# WWE 2K15 runtime stability notes

## Scope

This file tracks the current ReXGlue build while it is brought up and stabilized, alongside the longer-term native-renderer work in [NATIVE_PORT_PLAN.md](NATIVE_PORT_PLAN.md). Keep addressing concrete crashes, missing compatibility behavior, and rendering defects in this build; do not label a noisy allocator probe a game-breaking failure without checking its call arguments and whether the game recovers.

## Milestones

1. **Identify the game build.** Record the region, title update, executable size, and SHA-256 of the user's `default.xex`. Keep the executable and all game data local and out of Git.
2. **Establish code generation.** Pin the ReXGlue SDK revision, generate code from the exact executable, and capture every unresolved call and configuration requirement.
3. **Build a minimal runtime.** Compile and launch without game-specific fixes. Save startup logs and classify each failure by subsystem.
4. **Reach stable gameplay.** Bring up menus, a single match, entrances, and arenas separately. Record graphics faults, missing assets, and timing issues by scene.
5. **Improve rendering and performance.** A/B one setting or patch at a time, with each experimental patch behind a disabled-by-default flag until verified.
6. **Package the project.** Document toolchain and setup steps; package code and required notices only, never game data.

## First diagnostic pass

Before changing engine behavior, capture:

- Exact Xbox 360 release, region, title update, executable size, and SHA-256.
- ReXGlue version/revision, enabled graphics backend, and the generated game configuration.
- Code-generation result, unresolved calls, warnings, and build result.
- Startup log, first visible screen reached, and any crash location or GPU/device-loss message.

If the game reaches rendering, record one repeatable scene and test one variable at a time. For black screens, distinguish device-loss/fence errors, loading waits, first-use shader compilation, and mid-scene render flashes before changing settings.

## Current baseline (2026-10-16)

- Input: `default.xex`, XEX2 format, 24,133,632 bytes; SHA-256 `1AAF88A63A57AD3435E5DB6B228809D06929A07BD8CB7296CFBA0D5A903E0503`.
- ReXGlue metadata: Title ID `545408B6`, Media ID `77BB6C49`, executable version `0.0.0.4`, file time `2014-09-22 14:50:27 UTC`.
- SDK: ReXGlue `0.10.0.2-dev.gc94f5eb`, built locally with Vulkan enabled. Tracy was disabled after an SDK link failure; no game code was changed for that.
- First codegen pass found four unresolved direct branches. After adding their targets and a broader scan of missing data-pointer, constructed-address, and merged functions, codegen completed with 568 function hints in the local config. One false candidate that split a function was removed after the SDK reported a branch crossing it.
- The first runtime launches exposed three additional unregistered calls (`0x83455738`, `0x82C1A970`, `0x82C155E0`) and one more target (`0x824700C8`) from the wider function sweep. These addresses are now registered.
- Codegen emitted three conditional branches as fatal despite their labels being present inside the same generated C++ function. `port/tools/patch_codegen.py` repairs only these cases to local `goto` statements and runs automatically between codegen and compilation. The latest Windows build completed successfully.
- The initial launcher omitted ReXGlue's Xenos GPU plugin and attempted to write shader cache under `Documents\wwe2k15`, which raised `filesystem_error: create_directories: Access is denied.` The target now stages and loads `rexgpu-xenos.dll`, and the launcher directs user data/cache to its local `user_data` folder.
- A hidden diagnostic run stayed alive for 45 seconds without a fatal error or shader-cache permission error; the log confirmed the Intel D3D12 adapter and shader storage initialized.
- A later runtime log showed that the game also uses the guest Xbox `cache:` partition (`EditEx.cac`, `CAR.cac`), separate from the host shader cache. `Wwe2k15App` mounts `cache:`, `cache0:`, and `cache1:` at `user_data/xbox_cache`; the subsequent log confirmed this mount.
- Direct executable startup now discovers game files from adjacent `assets`/`game` folders or `%USERPROFILE%\Desktop\WWE2K15`; ReXGlue supplies its normal platform user folder unless overridden. CMake copies the runtime TOML beside the executable so its settings load on double-click.
- The first direct-executable launch produced a black screen because log `wwe2k15_019.log` had no GPU adapter initialization: only the batch launcher had selected `xenos`. `Wwe2k15App::OnPreSetup` now selects the Xenos GPU plugin automatically when no plugin is specified. The rebuilt executable includes this change; verify the adapter initialization on the next run.
- The user subsequently confirmed reaching a 1v1 match, at approximately 3 FPS. The captured `wwe2k15_018.log` identifies Intel UHD Graphics (`0x8086:0xA788`), 6,252 invalid texture-fetch warnings, and 4,964 guest heap allocation failures before the user closed the game. The supported `gpu_allow_invalid_fetch_constants` option was briefly enabled, but is now disabled because ReXGlue warns that it may render invalid fetches incorrectly.
- The runtime config disables VSync to avoid frame-rate stepping. It now leaves the guest video mode at its default after the 640x480 experiment was followed by a crash on the RTX 5070 run. Invalid fetch constants remain rejected for rendering correctness.
- Log `wwe2k15_021.log` confirms Windows selected the NVIDIA GeForce RTX 5070 Laptop GPU. The run stops abruptly after repeated guest physical allocation failures; it contains no orderly shutdown or recorded fatal exception.
- At the time of the first capture, the failure log showed 3,330 `page count too big for requested range` allocations. ReXGlue already documented the matching 4 KiB ranged-allocation workaround for WWE '13. Applied the same title-specific offset handling to WWE 2K15 (Title ID `545408B6`); the patch is kept in `port/patches/rexglue-sdk-wwe2k15-memory-range.patch`. The follow-up below records the first run after rebuilding.
- The latest run (`wwe2k15_023.log`) confirmed a D3D12 crash: `DXGI_ERROR_DEVICE_HUNG` (`0x887A0006`) after about 65 seconds. The invalid-fetch bypass was enabled and produced no fetch warnings, but the GPU hung; reverted it. Added a `gpu_backend` runtime option to ReXApp and select Vulkan for WWE 2K15 to avoid the D3D12 path. The Xenos plugin supports both backends.
- Vulkan run `wwe2k15_024` closed normally after about 2.5 minutes, but warning logs rolled over repeatedly (more than ten 5 MiB files) due invalid texture fetches. Run `wwe2k15_025` with the bypass enabled also closed normally, with no invalid-fetch warnings, but still logged 3,333 `page count too big` allocation failures, 183 unimplemented XCTD compression queries, 12 missing `streaming_install.pck` requests, and three `Resolve region is empty` draw failures. The bypass therefore quieted the texture warnings without fixing the memory failures; because it can reinterpret invalid descriptors, the current build now disables it again while retaining Vulkan. The flicker and slow video need another run to determine whether the bypass affected the displayed artifacts.
- Log `wwe2k15_020.log` confirms the title initializes D3D12 on Intel UHD, then reports repeated physical heap allocation failures and unimplemented `XFileXctdCompressionInformation` queries. It also requests `GAME:\Sound\streaming_install.pck`, which is absent from the supplied game folder. These are separate compatibility/content issues; do not treat the missing streaming pack as confirmed required content.- The latest captured run, `wwe2k15_020.log`, initialized the Intel D3D12 adapter and closed normally, with no device-loss/fatal errors and no invalid-fetch warnings while the bypass was enabled. It still logged 3,330 physical-allocation range failures (`page count too big`), 3,878 parent-heap allocation failures, and 119 unimplemented XCTD-compression queries. These guest memory allocation failures are the strongest remaining evidence for the in-game instability and low frame rate.
- The completed pass reported seven functions larger than the SDK's 1 MiB code-size threshold: `0x82F2BFF0`, `0x82F39CD0`, `0x82F479B0`, `0x82F55328`, `0x82F624C8`, `0x82F6EE90`, and `0x82F7CB68`. Inspect these before treating generated output as ready to compile.
- `find_fptrs.py` suggested setjmp `0x833AF8F0` and longjmp `0x833AF5D0`. The generated PPC-to-C++ confirms the first saves the nonvolatile register context and the second restores it, so both addresses are enabled in `wwe2k15_config.toml`.

The SDK source confirms that the settings from the WWE 2K14 notes exist in this revision. Defaults are: `async_shader_compilation=true`, `gpu_allow_invalid_fetch_constants=false`, `readback_resolve="none"`, `render_target_path_vulkan=""`, `log_max_file_size_mb=5`, and `log_max_files=20`. These are SDK defaults, not measured game-run values.

## Follow-up after the memory and XCTD fixes (2026-10-16)

- An earlier experiment made `NtQueryInformationFile(XFileXctdCompressionInformation)` report “not compressed” for every host file and was later reverted: runs 031–032 hung at the first-menu load because the game then read genuinely compressed XCTD bytes as plain data. Keep the original `X_STATUS_INVALID_PARAMETER` result until VFS-side XCTD decompression exists. `port/patches/rexglue-sdk-xctd-compression-info.patch` is historical and must not be applied.
- The rebuilt game launched with Vulkan on the NVIDIA GeForce RTX 5070 Laptop GPU (run 026). It logged zero `page count too big for requested range`, invalid texture-fetch, or empty-resolve errors, compared with more than 3,300 range errors in run 025. It still logged 459 `failed to find contiguous range` messages during the short startup window. A trace shows the game repeatedly requesting decreasing large contiguous blocks, then continuing with smaller successful allocations; these messages may be its maximum-block probe rather than a fatal memory shortage. A full visible match has not yet been rechecked with this build.
- Runs 026–029 were short hidden diagnostics, not gameplay-performance measurements. Run 026 had one Vulkan “Skipping frame presentation due to async placeholder draw usage” warning. The logs cannot establish whether the flicker, video cadence, or 3 FPS issue is fixed.
- The game requests `GAME:\Sound\streaming_install.pck`, which is absent from the local dump. The supplied sound folder contains `Comm.pck`, `Game_Sound_E_S.pck`, and `Game_Sound_M.pck`; do not create a placeholder file. Confirm whether that request is optional or whether the dump is missing an installed content file before attempting to map it.
- The brief launch reconfirmed that the runtime initializes Vulkan on the RTX 5070. The available evidence does not yet establish stable entrances or match rendering at a target frame rate. Next, run a visible match on the rebuilt binary and capture a normal shutdown log; compare the flicker, video cadence, frame rate, and allocation behavior against run 025.
- Run 030 (the newest captured log) stayed alive for about 48 seconds with a clean shutdown, and logged only two `streaming_install.pck` open failures against twelve in run 025.

## First-menu load freeze and the XCTD answer A/B (2026-10-17)

- Visible tests on the rebuilt binary (runs 031-032, XCTD compression info returning "not compressed") stalled with an infinite first-menu load. The loader reached GPU pipeline creation, then froze after the audio streaming phase: no new kernel or filesystem activity while the process spun at 100 % CPU, and zero further pipeline creations after that point.
- Runs 024-025 (pre-fix) had churned the same loader stage for minutes: hundreds of pipeline creations interleaved with `XFileXctdCompressionInformation` failures. That activity never completed within the observed window back then either, so the freeze was introduced with the XCTD patch, not older behavior.
- Returning `not compressed` for genuinely XCTD-compressed files makes the guest read raw compressed bytes and spin silently in its own decompressor path. The answer cannot salvage real compressed data either way.
- Runs 033-034 verified the A/B conclusion: reverting the SDK patch to the original error behavior restored loader progress. Without trace logging (heavy logfile rotation slowed run 033 measurably), run 034 (base configuration, no trace) completed the first menu load with active rendering: 358 graphics pipeline creations and the usual invalid texture-fetch warnings while the invalid-fetch bypass stayed disabled.
- `port/patches/rexglue-sdk-xctd-compression-info.patch` is therefore NOT applied in the current SDK tree. Long-term fix candidates, in order: implement transparent XCTD decompression in the VFS for compressed streams (the only answer matching Xbox kernel behavior), or study the game-side branch that reacts to the query result before touching the answer semantics.

## First stable visible match (2026-10-17, run 034)

- With the XCTD patch reverted, the first menu completed loading and a 1v1 match ran at roughly 60 FPS on the RTX 5070 Vulkan build. This is the first measured stable gameplay milestone on the Windows prototype; the historical ~3 FPS symptom from the pre-memory-fix Intel D3D12 build no longer reproduces.
- In-game audio worked during the match, which supports treating the missing `streaming_install.pck` as non-critical content.
- Match flicker remains unresolved. Run 034 had no `Resolve region is empty` or async placeholder-draw skips, but logged 105,214 invalid texture-fetch warnings (bypass off), 4,281 `page count too big` failures, and 589 contiguous-range probes. Run 035 tested the invalid-fetch bypass: it silenced the warning flood without removing flicker. `readback_resolve="fast"` was separately tested in run 036 and crashed Vulkan; do not use it. Next investigation should trace invalid fetch descriptor handling and presentation/frame pacing rather than toggling these rejected settings again.
- Note: the runtime config changes stay local to `port/out/build/win-amd64-release/wwe2k15.toml` between tests; the tracked `wwe2k15_runtime.toml` keeps only measured, verified settings.

- Run 035 enabled gpu_allow_invalid_fetch_constants: it completed cleanly with no invalid-fetch warning flood and 423 pipeline creations; the earlier visual A/B showed the match flicker remained but no extra texture corruption. Run 040 on the RTX with bypass off produced 190,478 invalid-fetch warnings across 53.7 MB of rotated logs. The tracked runtime config now enables the bypass to reduce log/CPU overhead while the actual invalid-fetch decoding is investigated.

- Run 044/045: ported the existing NVIDIA fuzzy alpha-test epsilon to the Vulkan SPIR-V shader path and enabled it with `use_fuzzy_alpha_epsilon=true`; this is a targeted flicker fix, but no in-match visual verdict was captured in these shell-launched runs. Run 045 selected the RTX 5070 and produced zero invalid-fetch warnings with the bypass enabled.
- Run 045 also confirmed the old Documents pipeline-cache path could not be opened for writing. `Wwe2k15App::OnConfigurePaths` now points `cache_root` beside the executable, and Vulkan successfully created `cache/shaders/shareable/545408B6.fbo.vk.xpso`; verify warm-start shader reuse and match pacing in a visible session.

## Vulkan presentation pacing instrumentation (2026-10-18)

- Log 062 showed the Vulkan backend skipped at least one frame because it used an asynchronous placeholder pipeline. It did not record how often this happened, so that warning alone could not explain the reported slow video or match flicker.
- The local Xenos/Vulkan plugin now emits a low-rate sample every five seconds of active swap traffic: swap requests and frames skipped while shader pipelines are pending. The change is tracked in `port/patches/rexglue-sdk-vulkan-pacing-stats.patch` and shipped in the current local `rexgpu-xenos.dll`.
- Use the next visible match/video session to compare swap-request cadence with async-skip counts. This telemetry can rule shader compilation in or out; it does not itself fix or prove an FPS improvement.
## streaming_install.pck investigation (2026-10-16)

- The game opens `GAME:\Sound\streaming_install.pck` shortly after GPU bring-up (about 1.3 s after Vulkan adapter init in run 030), receives `X_STATUS_NO_SUCH_FILE` (0xC000000F), and keeps running with degraded streamed audio; every captured run shut down cleanly afterwards.
- The failure is a read request, not a write attempt. `GAME:` is mounted through `Runtime::SetupVfs` as a `HostPathDevice` whose read-only flag follows the `allow_game_relative_writes` cvar (default disabled). On that device, an open-for-write fails with `X_STATUS_ACCESS_DENIED` (0xC0000022) or falls through `CreateEntry`, while a `FILE_OPEN`/`kOverwrite` open of a missing file returns exactly `X_STATUS_NO_SUCH_FILE` from `VirtualFileSystem::OpenFile`. The observed status therefore matches a plain read open.
- The series precedent supports real content, not a generated cache: the PC releases of WWE 2K22 through 2K26 ship `sound/streaming_install.pck` as an authored 265-563 MB Wwise streaming bank.
- Conclusion: the pack is expected authored content, the dump does not include it, and its absence degrades streamed audio without crashing. Do not create a placeholder file. If the file is ever available from the user's own copy of the game, mount it beside the dump rather than putting a hand-made file in the game folder.

The working manifest contains a machine-local path to the user's XEX and is ignored by Git. Start from `port/wwe2k15_manifest.example.toml` and point it at your own game files on another machine. The generated C++ is large and remains ignored.

## Full match log review (2026-10-18, run 061)

- The RTX 5070 Laptop GPU / Vulkan session reached normal window close and `Execution complete`; the log contains no fatal renderer or process termination error. This confirms a complete match session, but the log does not record frame-time or FPS measurements.
- Startup emitted 4,281 `page count too big` allocator-probe messages and 4,858 parent-heap wrapper messages in a short burst around 20:12:56.46. These probes are expected failed candidates; changing their log level does not alter allocation results and should reduce log overhead, but the burst lasted only milliseconds and is not evidence for the reported in-match stutter.
- 577 contiguous-range search failures, 189 XCTD compression-info queries, and 12 missing `streaming_install.pck` requests were also logged. Preserve the XCTD error return (the “not compressed” experiment caused the first-menu hang). The latest streaming pack requests did not prevent normal session shutdown.
- The run created several graphics pipelines during transitions. Persistent shader cache already exists locally, but the log alone cannot tell whether remaining stutter comes from shader compilation, video decode, or frame pacing. Do not claim a smoothness fix until frame-time capture and a visual A/B confirm it.
- Runtime diagnostic cleanup: `port/patches/rexglue-sdk-log-noise.patch` demotes only repetitive expected allocator-probe/wrapper messages and repeated unsupported XCTD query messages to debug. It does not change allocation or file-query results.

## Pre-match video capture review (2026-10-18, run 066)

- The user capture is 6.5 seconds, H.264 at 30 FPS (1918x1072); 185 of 195 captured frames have distinct full-frame hashes. The capture shows match selection transitioning through the Night of Champions visual into the loading screen. It does not show a sustained 5-FPS renderer stall during those 6.5 seconds.
- The matching Vulkan log reports about 56 swaps/s at 20:55:41, then about 30 swaps/s from 20:55:46 through 20:56:11, with 0 shader-pending skips in those intervals. It climbs back toward 60 swaps/s by 20:56:21. The game closes normally at 20:56:25.
- The capture itself overlaps the ~60-swap/s interval. Sampled `.bk2` assets report a nominal 29.97 FPS stream, so the later ~30-swap/s interval may be the title's intended movie/loading cadence rather than a decode slowdown. The sampled files expose no video decoder to FFmpeg, so their metadata does not reveal decoded-frame timing.
- This ties the observed slow section to the video/loading transition, not shader compilation or a Vulkan device failure. Swap requests do not measure Bink frame decode or guest frame time, so they cannot prove whether the movie is decoded slowly, the guest is deliberately pacing at 30 Hz, or the title's loading work is starving rendering.
- Next diagnostic: correlate guest CPU/frame time and Bink2 (`.bk2`) frame updates with Vulkan swaps across the same transition. Avoid changing Vulkan shader settings based on this capture; the measured intervals had no pending-shader skips.

## Local launcher

- `Lancer WWE 2K15.bat` opens the native Win32/D3D11 Dear ImGui launcher. It validates and remembers the selected `default.xex` folder, starts the Xenos plugin with Vulkan and SDL input, and provides controls, mods, saves, about, and log-folder pages. Game data stays outside the repository; user data goes under `%LOCALAPPDATA%\WWE2K15`.
- CMake builds and stages `WWE 2K15 Launcher.exe` and the batch entrypoint beside `wwe2k15.exe`. Set `WWE2K15_IMGUI_DIR` to the SDK's `thirdparty/imgui` when using an installed SDK package.

## Porting rules

- Do not assume WWE '12 or WWE 2K14 addresses, function names, loader hooks, compression paths, or ReXGlue settings apply to WWE 2K15.
- Keep all game-specific patches behind explicit flags until their correctness and payoff are measured.
- Preserve logs and exact configuration for each comparison run.
- Verify native replacements against the original behavior before enabling them by default.
- Never commit the game executable, disc image, extracted game assets, save data, or copyrighted audio/video.

## Open items

- Exact WWE 2K15 Xbox 360 executable/build and title update.
- ReXGlue SDK revision and whether this project needs SDK changes.
- Visible rendering, menu entry, and a repeatable first match.
- Repeatable test scene and baseline performance.

## Hot-thread profile during intro videos (2026-10-20)

- One guest thread sits at about 97 % CPU while Vulkan still reports about 60 swaps/s and 0 shader-pending skips. All 12 `cdb` stack samples of that thread (`port/tools/sample-hot-thread.ps1`) land in `sub_82BA4738`, called by `sub_82BA8208` from `sub_82BA5FA8` (the function that calls `VdSwap`), which is the title's render thread.
- `sub_82BA8208` is the D3D command-ring wait loop: it spins on `sub_82BA4738` (a `db16cyc` delay loop plus a tick-timeout check) until the ring read pointer written back by the GPU command processor passes a target. High CPU on this thread is therefore a spin-wait, not decode work, and does not by itself show a CPU bottleneck. The wait is for the command processor / GPU side.
- During the same run the process' GPU 3D engine was only at about 26-61 %, so the GPU was not saturated in the intro. The command-processor thread was at about 20 % CPU.
- The machine was running on battery (`Win32_Battery.BatteryStatus = 1`). Earlier WWE '12 work showed this laptop throttles or drops the RTX 5070 on battery, so slow-video tests must be done plugged in.
- Not yet measured: the slow scenes themselves (menu videos, nameplates). Next step is to run the same sampler there and watch the ring read-pointer writeback cadence in the Xenos plugin.
- Symbolizing: relink the existing objects with `/debug /map /opt:ref /opt:icf` into a scratch folder (the `.text` size matches the shipped exe), then `llvm-symbolizer --relative-address --obj=<relinked exe>` on the `wwe2k15+0x...` offsets from `cdb`. The relinked exe is only for symbols and is not run.

## Video decode thread and AVX2 (2026-10-20)

- In a slow menu video the render thread was still spin-waiting on the command ring, but a second guest thread ran at about 83 % CPU inside the title's video decoder (`sub_8353xxxx`, reached from `sub_8352EE10`). Its leaf frames were `rex::ppc::simde_mm_srav_epi16` / `sllv_epi16`, which widen to 32-bit and use variable 32-bit shifts; the build targeted `x86-64-v2`, so SIMDe emulated those with scalar loops.
- The helpers are about 3x faster at `x86-64-v3` in a microbenchmark (0.56 s vs 0.19 s). The presets now use `x86-64-v3`, so the build requires an AVX2 CPU (Haswell / Zen 1 or newer).
- Same scene on the v3 build: the decoder thread dropped to about 14 % CPU and mostly sleeps in `KeDelayExecutionThread`, but swaps in the slow video only rose from about 20/s to about 26/s. A second limiter remains on the render/command-processor side (GPU 3D engine only about 5 % busy, the render thread spinning in `sub_82BA8208`). Next: sample the command-processor thread and the Xenos plugin during the video.
- Building without re-running codegen: configure a fresh build dir, then replace the `codegen` and `patch_codegen.py` custom commands in its `build.ninja` with a no-op so the existing `port/generated` sources are reused.

## Timer resolution and the command-processor sleeps (2026-10-20)

- In the slow menu video the Vulkan command-processor thread (`t17220`) was asleep in `Sleep` in all six `cdb` samples while the render thread spun in `sub_82BA8208`, waiting for the ring read pointer. SDK `CommandProcessor` waits with `Sleep(wait / 0x100 ms)` in `WAIT_REG_MEM` when the `vsync` cvar is true, and the GPU VSync worker also loops on `Sleep(1 ms)`.
- The SDK never raises the Windows timer resolution (no `timeBeginPeriod`), so each 1 ms sleep lasts about one 15.6 ms tick. `Wwe2k15App::OnPreSetup` now calls `timeBeginPeriod(1)`. Effect in game not yet measured.
- The cvar is named `vsync` (not `gpu_vsync`). `gpu_vsync = false` in `wwe2k15_runtime.toml` therefore does nothing; do not rely on it. Setting `vsync = false` would also change the vblank interval to 1 ms and may make the game run too fast, so test it separately.


## Title Update 4 and DLC (2026-10-20, branch `tu4`)

- The user's TU package `tu00000004_00000000` is STFS `LIVE`, title `545408B6`, media `77BB6C49` (same as the dump), version 4. It holds `default.xexp` plus `plist360.*` and `pac/...` overrides. The eight DLC packages are `LIVE` marketplace content for `545408B6`.
- ReXGlue already applies a sibling `default.xexp` in `UserModule::LoadFromFile`, for both codegen and runtime: the image goes from executable version `0.0.0.4` to `0.0.6.4`. As in the WWE '13 recomp, the code is therefore generated from the TU-patched image, and this build needs TU4 beside `default.xex`.
- Function hints were remapped from the base image to TU4 by matching masked instruction sequences in dumps of both loaded images: 541/568 matched. Three of the four base tail-call targets matched exactly the three unresolved branches reported by the first TU4 codegen. Ambiguous 16-byte stubs and unmatched addresses stay commented in `wwe2k15_config.toml`. A scan of TU4 data words for code pointers that follow a function end added the rest, including `0x82362BF8` (a runtime FATAL); kernel import thunks (`0x8353xxxx`) were excluded.
- Hook addresses moved: render ring wait `sub_82BA8208` -> `sub_82BB0180`, poll `sub_82BA4738` -> `sub_82BAC6B0` (only a data offset differs), setjmp/longjmp `0x833AF8F0`/`0x833AF5D0` -> `0x833B90F0`/`0x833B8DD0`.
- `Wwe2k15App` mounts `UPDATE:` on `<game>\update` (TU data files) and installs each STFS package in `<game>\dlc` once with the SDK's `ContentManager::InstallContent` (extracted under user data `0000000000000000\545408B6\00000002`). The automatic game-folder search now only accepts a folder that also holds `default.xexp`.
- `port/patches/rexglue-sdk-xam-content-root-name.patch` fixes a dangling `string_view` in deferred `XamContentCreate`: opening the DLC crashed with `Dispatch thread: deferred completion threw 'Invalid UTF-8'`. The WWE '13 recomp carries an equivalent fix.
- Loading a match hit `0x82350878`, one of 211 16-byte virtual-call thunks (`lwz r12,0(r3); lwz r11,x(r12); mtctr; bctr`). They are reached indirectly and not referenced from data, so the pointer scan missed them; the base config had all 211 registered. The 8 missing TU4 thunks (the ambiguous stubs above among them) are now registered.
- Verified on TU4 + DLC with keyboard input: DLC notice ("THE ASCENSION tag team is available"), main menu, the character select with DOWNLOADED SUPERSTARS enabled (greyed without DLC), and a complete match load (John Cena vs Roman Reigns, Night of Champions) at ~60 swaps/s with no FATAL.
- First TU4 + DLC run on the RTX 5070 Ti: TU applied, `UPDATE:` mounted, 8 DLC installed and enumerated, title demo at ~60 swaps/s, no FATAL in 100 s. Menus, DLC roster content and a match are not yet verified on TU4.

## Victory-scene corruption and the invalid-fetch bypass (2026-10-20, TU4)

- With `gpu_allow_invalid_fetch_constants = true`, the user saw large visual bugs in victory scenes. Captures showed a WrestleMania XXX sign (the title-screen demo arena) drawn in the Royal Rumble arena, a black patch on Roman Reigns' arm, and red-tinted crowds. Accepting "invalid"-type texture fetch constants samples whatever texture was last at that address.
- With the bypass off, two Cena vs Reigns matches (Royal Rumble and Elimination Chamber arenas) ran to the victory scene and the Exit/Rematch menu with none of those defects, no FATAL, and ~60 swaps/s in play (30 during the usual cutscene/transition intervals).
- The bypass had been enabled only to stop a log flood (~16,000 warnings and 40+ MB of rotated logs per match). `port/patches/rexglue-sdk-texture-fetch-log-once.patch` now logs each distinct rejected constant once (cap 256), so a full match logs ~26 such lines. The tracked runtime config sets `gpu_allow_invalid_fetch_constants = false`.
- `port/patches/rexglue-sdk-empty-resolve.patch` treats an empty resolve region as a successful zero-sized copy (as the console and the WWE '13 recomp do) instead of failing the draw: 48 `Resolve region is empty` / `PM4_DRAW_INDX_2 Failed in backend` pairs per match went to 0.
- Not yet tested: `execute_unclipped_draw_vs_on_cpu = true` (Xenia default, enabled by the WWE '13 recomp for an EDRAM wrap). Try it next if match flicker remains.

## Black screens during entrances (2026-10-20, TU4)

- A rejected "invalid"-type fetch constant first appeared just before an entrance black-out: `81404800 10A69086 000B209F ...`, a 160x90 8888 texture (1/8 of 1280x720), i.e. a post-processing render target. The SDK now accepts invalid-type constants whose base page holds GPU-written (resolve) data, cvar `gpu_allow_invalid_fetch_of_gpu_written` (default on), while still rejecting the rest (stale asset memory caused the victory-scene corruption). Rejected constants per match dropped from 26 to 2, and the victory scene stayed clean.
- That did not remove the black-outs. Captures every 5 s show ~20 s of black between Cena's and Reigns' entrances, plus ~10 s inside Reigns' entrance, at 30 swaps/s and with no GPU error. The black-out starts exactly when the title opens `GAME:\Sound\streaming_install.pck` and `dlcN:\sound\streaming_install.pck` (all absent). The leading hypothesis is that entrance loading waits on that missing streamed content; not confirmed. Compare with a console capture or a dump that contains the pack.

## Entrance black-outs fixed with the FSI render-target path (2026-10-22)

- Frame counters added to the Vulkan presentation sample showed 4,000-11,000 draws and ~80 resolves per frame during the black entrance frames: the game renders the scene, the output is lost. Removing the titantron videos did not change it (not Bink), and `readback_resolve=full` did not either.
- `render_target_path_vulkan = "fsi"` (EDRAM emulated with fragment shader interlock) removed the black-outs: Reigns' entrance, The Shield titantron and the walk to the ring render correctly, with 0-2 black frames per run left at load transitions, ~30 swaps/s in entrances and ~60 in the match (RTX 5070 Ti). The host render-target path is the likely culprit (EDRAM aliasing between formats); FSI is now set in the tracked runtime config.
- A full Cena vs Reigns match under FSI (RAW arena) ran 10 min to the victory scene and the Exit/Rematch menu: 1 black capture out of 120 (a load transition), no FATAL, clean victory scene. Still unchecked: GPUs without fragment shader interlock (AMD), which fall back to the host-RT path.

## Mode sweep under FSI (2026-10-22, TU4 + DLC, RTX 5070 Ti, keyboard)

All ran with no FATAL and reached their end screens:
- One on One, I Quit (Cena vs Reigns, SummerSlam): entrances with titantron, match, victory, Exit/Rematch. 2 black captures of 100 (load transitions).
- 2K Showcase: archive Bink videos (1280x720, smooth), CM Punk Money in the Bank entrance and match load.
- Creations > Superstar: 3D editor, head/hair parts and their thumbnails (render-to-texture) correct.
- Universe: RAW card, Divas match (AJ Lee vs Nikki Bella, AI vs AI) to victory and Next Match; tag rivalry match (R-Truth & Xavier Woods vs Ryback & Curtis Axel) to victory and Next Match.
- Swap rate alternates between 60 and 30 per second in matches (30 dominates in the tag match). The original Xbox 360 game targets 30 FPS in matches, so 30 is not necessarily a regression; an uncapped 60 FPS mode would need a game-side patch.
- Not tested: Who Got NXT, Hell in a Cell / ladder / Royal Rumble, MyCareer, saves of created content.

## Optimisation CPU (2026-10-26)

- Patchs SDK repris du port WWE '12 : `rexglue-sdk-timer-sleep.patch` (fil TimerQueue qui tournait à vide),
  `rexglue-sdk-cores-warning.patch` (avertissement « Too few processor cores » une seule fois sur les PC < 6 fils),
  `rexglue-sdk-thread-name.patch` (course sur le nom d'un thread).
- `wwe2k15_render_wait_yield` activé par défaut (jeu et launcher, clé `gpu_wait_yield`). Mesure A/B, 1v1 Cena vs
  Reigns, 720p : entrées 30 fps dans les deux cas, 170 % → 84 % d'un cœur ; match 60 fps dans les deux cas,
  200 % → 115 %, écart max 20,4 → 18,6 ms. Le fil de rendu (XThread F8000058) passe de ~92 % à ~1 %.
- Reste : « GPU Commands » ~53 % d'un cœur en match (fil du SDK) ; chargements (le budget du chargeur de WWE '12,
  sub_82819258, n'a pas d'équivalent évident dans 2K15 : à chercher en échantillonnant un chargement).
- Processeurs : le jeu et le SDK sont compilés en `x86-64-v2` (SSE4.2, processeurs de 2009 et après) au lieu de
  `x86-64-v3` (AVX2), qui aurait planté au lancement sur les vieux processeurs et certains Pentium/Celeron. Aucune
  instruction AVX écrite à la main dans le SDK ni le code généré. Mesuré en match : 113 % d'un cœur (v2) contre
  115 % (v3), 60 fps. Le SDK produit maintenant ses PDB (`/Z7`, `/DEBUG /OPT:REF /OPT:ICF`, même code).
- « GPU Commands » échantillonné en match (PDB + llvm-symbolizer) : 35 % d'attente, puis WriteRegister (~15 %),
  UpdateBindings (7 %), IssueDraw. `rexglue-sdk-gpu-register-writes.patch` : les paquets type 0 consécutifs
  passent par WriteRegisterRangeFromRing (copie groupée des constantes de shaders) et la recherche du registre
  inconnu n'est faite que si le journal gpu est en debug. GPU Commands 52 % → 41 %, jeu 112 % → 100 %, 60 fps.
- Textes : `src/text_hook.cpp` crochète l'inflate du jeu (sub_82B529A0) et remplace « console Xbox 360 » par
  « PC » dans les 5 langues (23 textes au démarrage, vérifié à l'écran en français). Xbox Live / Store inchangés.
