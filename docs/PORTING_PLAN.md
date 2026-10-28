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

## Superstar Threads / readback des resolves (2026-10-26)

- Symptôme (rapport Discord) : tenues et peau en bandes de couleurs brouillées dans Panoplie Superstars et
  en match. Même cause que WWE '12 (commit b24b370 de ce projet) : le jeu compose la tenue sur le GPU, fait un
  resolve, puis le CPU relit la texture pour la compresser en DXT ; sans readback il compressait une mémoire vide.
- `readback_resolve` du SDK était à "none". "some"/"fast" perdaient le device Vulkan dans le match de démo :
  `EvictOldReadbackBuffers` détruisait des tampons encore écrits par une soumission en vol (et tous au-delà de
  256 clés). "full" seul : 37-56 fps en match.
- `rexglue-sdk-readback-resolve.patch` : suivi `submission_written` + éviction seulement des tampons terminés ;
  cvar `readback_resolve_formats` (2,6,15 = k_8, k_8_8_8_8, k_4_4_4_4 ; les 25/26 = 16_16/16_16_16_16 sont la
  scène) ; pas de readback pour une cible redessinée 60 images de suite (`readback_resolve_repeat_limit`, ~38 par
  image en match) ni pour une cible 16:9 de 1024 px et plus (jugée par sa forme : le niveau 0 d'une tenue fait
  1024x1024 = 4 Mo ; l'exclure par la taille laissait les gros plans brouillés, vu par l'utilisateur).
  Mesuré (même arène RAW) : match 60 fps, entrées 30 fps ; tenue « ROSE » correcte dans Panoplie.
- Options de codegen (skip_lr, *_as_local) : avec les huit, blocage au démarrage avant la 1re image (aucune
  ligne « Guest pacing »). Désactivées dans `wwe2k15_config.toml`, à réactiver une par une. Exe 112 → 81 Mo.

## Image disque (.iso) (2026-10-26)

- Demandé par 2-3 joueurs. `rexglue-sdk-disc-image.patch` (repris du port WWE '12) : `game_data_root` peut
  être un fichier `.iso` (XDVDFS), monté par le `DiscImageDevice` du SDK au lieu d'un `HostPathDevice`.
- Le code est généré depuis l'image TU4 : avec une ISO, `default.xexp`, `update\` et `dlc\` sont dans
  `game_update\` à côté de l'exe ; `OnLoadXexImage` superpose ce dossier au disque (overlay VFS) pour que
  le chargeur trouve `game:\default.xexp`. Le launcher a « CHOISIR UNE ISO… », détecte une `.iso` dans `game\`
  et installe la TU / les DLC dans `game_update\`.
- Testé : ISO fabriquée avec `port/tools/dev/make_xiso.py` (sans TU ni DLC, 7,6 Go), TU ajoutée par le
  launcher, jeu lancé depuis le launcher jusqu'aux écrans de chargement à 60 fps. Pas encore testé avec une
  ISO de joueur (XGD2/XGD3 à l'offset de partition, gérés par le SDK).

## (Rendu) Host render-target path ("rtv") on NVIDIA and AMD: entrance black-outs not reproduced (2026-10-26)

- The PC has two Vulkan devices: RTX 5070 Ti (device 0) and an AMD Radeon iGPU (0x1002:0x164E, device 1; select with `--vulkan_device=1`). The AMD iGPU does expose `VK_EXT_fragment_shader_interlock`, so with the default config it also runs the FSI path. Any `render_target_path_vulkan` value other than `fsi` selects the host render-target path (the SDK has no "rtv" value; `fbo` is only a documentation name). The SDK does not log which path it took.
- Test: `ab3.sh`-style run (title -> 1v1 Cena vs Reigns, RAW) with a capture every ~3 s through the entrances, TU4 + DLC, current build.
  - NVIDIA, `--render_target_path_vulkan=rtv`: entrances (Cena, Reigns, The Shield titantron) render correctly; 4 black captures of 30, isolated, load transitions.
  - AMD iGPU, default (FSI): entrances correct (3-4 fps, the iGPU is slow).
  - AMD iGPU, `--render_target_path_vulkan=rtv` (real host-RT path on AMD): entrances correct, one black stretch of ~4 captures between Cena's and Reigns' entrances = the load transition (same spot as the 2026-10-20 note), Reigns' entrance then renders.
- So the "everything black, thousands of draws lost" entrance bug of 2026-10-20/22 is not reproduced with the host path any more, on either vendor. Likely fixed since by other changes (invalid-fetch of GPU-written data, empty resolve, readback resolve), or it depends on a GPU/driver we do not have. No SDK change made.
- Next if an AMD tester still reports it: ask for the log and GPU model; a log line naming the selected render-target path would help (not added).

## 540p : étude sur le code, sans le PC (2026-10-26, Perf)

- Le jeu n'a pas de constantes 1280x720 faciles à compter : 105 `li ...,1280`, 58 `li ...,720`, 76 `li ...,640`
  dans le code généré, mais seulement 8 fonctions ont les deux (`sub_8228E420`, `82908AF8`, `82A363B0`,
  `82A368E0`, `82AB1390`, `832E5668`, `833B8DD0/90F0` dont les deux dernières sont setjmp/longjmp, faux positifs).
  Aucun comptage fiable de « sites de création de cibles » par recherche de constantes.
- La bibliothèque D3D de la console (`sub_82BBAD00`, `82BBB280`, `82BC02D8`, `82BBE760`) appelle
  `VdQueryVideoMode` / `XGetVideoMode` / `VdSetDisplayMode` : le jeu lit le mode vidéo de la console
  (largeur, hauteur, widescreen, hi-def). Les jeux 360 adaptent d'ordinaire leur taille de rendu à ce mode
  (480p, 720p, 1080i), donc le jeu sait peut-être déjà rendre en 848x480 ou autre.
- Le SDK expose ce mode par deux cvars : `video_mode_width` (1280) et `video_mode_height` (720) (SDK
  `src/kernel/xboxkrnl/xboxkrnl_video.cpp`, `src/ui/window.cpp`). Le plugin Xenos ne fait que multiplier par
  `resolution_scale`; il ne choisit pas la taille des cibles, c'est le jeu qui le fait.
- Hypothèse à tester (aucun code à écrire) : lancer avec `--video_mode_width=960 --video_mode_height=540`
  (puis 848x480 = vrai mode SD 16:9 de la console). Commande prête :
  `bash port/tools/dev/ab3.sh res540 <dossier exe> --video_mode_width=960 --video_mode_height=540`
  et lire dans le journal la ligne « Frame activity ... frontbuffer ... WxH » ainsi que les captures
  (menus, entrées, ombres, interface). Si la taille du frontbuffer change, le jeu suit le mode : faisable
  par simple réglage + option du launcher. Si elle reste 1280x720, il faudra chercher les tailles de cibles
  dans `sub_82BBAD00` et la création de surfaces D3D (`RB_SURFACE_INFO`, pitch), ce qui est risqué
  (post-traitement, ombres et interface sont probablement calibrés en 1280x720).
- Estimation : faisable si le jeu suit le mode vidéo (test d'une minute), risqué sinon, pas raisonnable si
  il faut patcher plusieurs dizaines de sites. Rappel : le goulot de 2K15 est le processeur, le 540p ne
  sert qu'aux cartes graphiques intégrées.
- Résultat du test (fait par (Main), écran titre, ~1 min) : avec `--video_mode_width=960 --video_mode_height=540`, le
  journal garde « frontbuffer 115D6000 1280x720 » (3 mesures sur 3) et l'image est identique. 2K15 ignore le mode
  vidéo, au moins aux menus : le 540p par simple réglage ne marche pas. La suite (patcher les tailles de cibles)
  demanderait beaucoup de sites : pas de chantier sans l'accord de l'utilisateur.

## (Rendu) "BaseHeap::Alloc failed to find contiguous range" is a probe, not a shortage (2026-10-26)

- Every launch logged ~460-600 `[error] [sys] BaseHeap::Alloc failed to find contiguous range` (459 in one burst on the main guest thread right after the pipeline storage load, ~62 more ~40 s later on another thread).
- One launch to the title screen with `[log.levels] krnl = "trace"` and `--log_noisy=true`: all 459 come from `MmAllocatePhysicalMemoryEx` (physical heap, 64 KB pages, min=0 max=0xffffffff, align 0x10000). The game asks for 0x1e000000 (480 MB), then drops 1 MB per refusal until a size fits (0x2a00000 = 42 MB), then frees that block at once (`MmFreePhysicalMemory`): it measures the largest free physical block. The 0 return is handled by the game. No `NtAllocateVirtualMemory` failed. The second burst was not traced (after the measurement window), probably the same probe.
- Fix: `rexglue-sdk-log-noise.patch` now logs this miss at debug (as it already did for the sibling messages); new `rexglue-sdk-alloc-fail-warning.patch` logs one warning, the first time `NtAllocateVirtualMemory` really fails, so a real shortage on a tester's PC is still visible. SDK rebuilt and installed, game relinked; not run after the change (check the next log: the error lines should be gone).

## Petit processeur simulé (2026-10-26, Perf)

- Affinité posée par `(Get-Process wwe2k15).ProcessorAffinity` après le lancement (`start /affinity` n'a eu aucun
  effet : masque resté FFFF). Scripts : `port/tools/dev/smallcpu.sh <tag> <masque> <exe dir>` (démo),
  `AFF=<masque> bash ab3.sh|loadsample.sh ...`.
- PC de test (RTX 5070 Ti, cœurs rapides). Démo de l'écran titre : 8+ cœurs 60 fps ; 4 cœurs 60 fps (écart max
  19-33 ms) ; 2 cœurs 48-59 fps, un écart de 354 ms.
- Match complet (ab3), 4 cœurs : entrée 30 fps, match 60 fps, écarts 18-33 ms, comme sans limite. 2 cœurs : entrée
  30 fps avec pointes de 235 ms, début de match 50-59 fps avec écarts de 33-35 ms, une pointe de 394 ms à la
  transition.
- CPU par fil en match : « GPU Commands » 45 % (4 cœurs) / 52 % (2 cœurs) d'un cœur, le plus lourd de loin ; puis
  trois fils du jeu à 12-16 % chacun, audio ~3 %. Total ~100-120 % d'un cœur. Sur un vrai petit processeur
  (cœurs 2x plus lents), « GPU Commands » approcherait 100 % d'un cœur : il serait le goulot, car il est
  séquentiel. Piste principale : alléger ce fil (SDK `command_processor`, UpdateBindings, IssueDraw).

## (Rendu) FSI vs host render-target path: GPU cost, cold shader cache (2026-10-26, RTX 5070 Ti)

Demo match from the title screen (`port/tools/dev/gpu_matrix.sh`, `gpu_compare.sh`; `nvidia-smi` every 0.5 s over 30 s). Warm cache = the dev PC's. Host = any `render_target_path_vulkan` other than `fsi`.

| run | GPU load avg (max) | GPU power | CPU | fps / worst gap |
|---|---|---|---|---|
| host 720p | 30 % (35) | 60 W | 101 % | 60 / 18.7 ms |
| FSI 720p | 48 % (60) | 71 W | 95 % | 60 / 18.9 ms (one 35.8 ms) |
| host 1440p | 46 % (51) | 91 W | 111 % | 60 / 18.8 ms |
| FSI 1440p | 73 % (77) | 106 W | 106 % | 57-60 / 19.3 ms (two ~34 ms) |

- The host path costs about 35-40 % less GPU time than FSI at both resolutions (30 vs 48 %, 46 vs 73 %); CPU is the same.
- Cold shader cache (exe copy with an empty `cache\`): FSI held 60 fps, 229 pipelines built during the run; host path dipped to 55-58 fps with ~34 ms gaps and built 418 pipelines (the host path needs about twice as many pipeline variants). `skipped for pending shaders` stayed 0 in every run; the demo only covers an easy scene, so this is a lower bound.
- Host path visual check (NVIDIA): entrances (see section above), match in RAW, and Superstar Threads attire (Adam Rose, clean, not scrambled) are correct. Victory scene and shadows were not compared side by side.
- Caches are per path (`545408B6.fsi.vk.xpso` / `.fbo.vk.xpso`): switching path restarts from a cold cache for that path.

## « GPU Commands » allégé : jeux de textures et constantes (2026-10-26, Perf)

- `rexglue-sdk-descriptor-reuse.patch` : (1) `UpdateBindings` réécrivait à chaque dessin les jeux de descripteurs
  textures/échantillonneurs (TODO d'origine « Reuse texture and sampler bindings if not changed ») ; il compare
  maintenant les vues/échantillonneurs au dernier jeu écrit de l'image et ne réécrit que s'ils changent (oublié à
  l'ouverture de chaque image). (2) La copie des constantes flottantes des shaders se fait par séries de
  constantes consécutives (un memcpy par série) au lieu d'un par constante ; sortie identique octet pour octet.
- Échantillon (match, 2 cœurs) : après le correctif 1, les lignes chaudes de `UpdateBindings` étaient la copie des
  constantes du vertex shader (ce qui a mené au correctif 2) ; viennent ensuite `ExecutePacket`,
  `UpdateSystemConstantValues`, `IssueDraw`, `log::gpu`.
- Mesures, 2 cœurs, match 1v1 : « GPU Commands » 52,4 % → 47 % d'un cœur, total 126 % → 119 %. Sans limite :
  GPU Commands 38,9 %, total 96 % (avant ~41 % / ~100 %), 60 fps match / 30 fps entrées, aucune perte de
  périphérique. L'image est identique à l'œil (arène RAW : textures, foule, ring corrects). Gain réel mais
  modeste (~10 % du fil) : bruit de mesure ±3 points.
- Reste à voir : `rex::log::gpu` apparaît dans les échantillons (journal à gpu = info ?), `ExecutePacket`,
  `UpdateSystemConstantValues`.
- `rexglue-sdk-gpu-register-writes2-level-cache.patch` (s'applique après `gpu-register-writes`) : le niveau de
  journal gpu est lu une fois (statique) au lieu d'un appel inter-DLL à chaque écriture de registre. Mesure à 2
  cœurs : 46,4 % contre 47 % pour « GPU Commands » : dans le bruit, gain d'environ 1-2 %. Après les deux
  correctifs précédents le profil de « GPU Commands » est plat (aucune ligne ne dépasse ~2 % des échantillons) ;
  d'autres gains demanderaient de réduire le nombre de dessins ou de paralléliser, pas de micro-optimiser.

## (Rendu) Issue #3: matches at 2-10 fps on an RX 6750 XT -> "auto" render-target path (2026-10-27)

- Reporter's logs (0.1.3-beta, 2560x1440, FSI): menus 60 fps; in the match 10,500-10,800 draws and ~70 resolves per frame at 2.5-4 fps, "GPU-ring wait 0%". The same per-frame work runs at 60 fps on the RTX 5070 Ti, so the GPU side is ~20x slower, not the CPU.
- Our own AMD Radeon iGPU (`--vulkan_device=1`) logs from 2026-10-26, same match, 720p: FSI 3-6 fps, host render targets 14-18 fps. AMD drivers serialize overlapping fragments for the interlock, so FSI is 3-4x slower than the host path there; on NVIDIA FSI costs ~1.6x GPU time (section above). The host path's entrances were checked on that iGPU on 2026-10-26 (correct, only load-transition blacks).
- `rexglue-sdk-render-path-log.patch` now also adds `render_target_path_vulkan = "auto"`: FSI, except on AMD (vendor 0x1002), which gets host render targets; "fsi" still forces FSI, any other value forces the host path. The startup log line names the choice ("auto, AMD GPU" / "auto, non-AMD GPU"). `wwe2k15_runtime.toml` uses "auto", and the launcher's "Rendu précis (FSI)" box now passes "auto" when ticked (still "rtv" when unticked). Checked: RTX 5070 Ti -> FSI, Radeon iGPU -> host.
- Not done yet: GPU time per frame (timestamp queries) in the 5 s log, so every report says GPU-bound vs CPU-bound. It touches `vulkan/command_processor.cpp`, which (Perf) is changing for the pipeline cache; to do after that lands.

## Issue #4 : Ryzen 5 PRO 4650G, Vega 7, pilote AMD 22.20 (2026-10-27, Perf)

- Journaux du testeur (`work_issue_logs\i4`) : 720p, chemin hôte (pas de FSI sur Vega). Menus 60 fps ; match
  30-37 fps avec 2 000-6 000 dessins par image et ~9 000 copies (resolves) par 5 s ; au chargement du match des
  écarts de 1,1 s, 1,5 s et 4,7 s (11,7 fps sur 5 s) pendant que 60-130 pipelines graphiques sont créés par
  seconde. 359 puis 498 créations de pipelines par lancement.
- Le SDK n'avait **aucun VkPipelineCache** : le pilote recompilait chaque pipeline à chaque lancement (le fichier
  .xpso ne garde que les descriptions). `rexglue-sdk-pipeline-cache.patch` ajoute un cache pilote persistant
  (`<cache>\shaders\<vendeur>-<appareil>-<pilote>.vk.pcache`, ~10 Mo, rechargé au lancement, sauvé au plus toutes
  les 30 s et à l'arrêt) et journalise le temps total passé dans `vkCreateGraphicsPipelines`.
- Mesure sur ce PC (iGPU AMD `--vulkan_device=1`, chemin hôte forcé `--render_target_path_vulkan=rtv`) : le pilote
  récent crée un pipeline en ~0,4 ms (439 pipelines = 162-204 ms), donc ici le cache ne change rien (écart max au
  chargement 296 ms → 131 ms, dans le bruit). Le gain n'est donc PAS prouvé ; il dépend du pilote 22.20 du
  testeur, dont la compilation semble 100 fois plus lente. À faire confirmer par le testeur (ligne « Saved the Vulkan
  driver pipeline cache ... ms in total » à la fin de la partie, puis 2e lancement).
- iGPU de ce PC (Raphael, 2 CU, bien plus faible qu'un Vega 7) : match à 15-20 fps avec le processeur à 38 % d'un
  cœur (« GPU Commands » 16 %) : c'est le GPU qui limite, pas le processeur. Anisotropie 0 au lieu de 4x : +2 à
  3 fps (~+12 %). Le goulot probable sur Vega 7 est le même : GPU/bande passante (résolutions + 2 000-6 000 dessins).
- Pistes : (1) anisotropie 0 par défaut pour les iGPU (préréglage « faible » du launcher) ; (2) mesurer le coût
  GPU par passe (timestamps GPU, prévu par (Rendu)) avant de toucher aux resolves ; (3) 540p impossible (voir
  plus haut).
- (later the same day) GPU time per frame is now logged: `rexglue-sdk-vulkan-gpu-time.patch` writes a timestamp at the start and end of every Vulkan submission and logs every 5 s `GPU time: busy N% of 5.0 s, X ms per presented frame (frames, submissions)` (cvar `vulkan_log_gpu_time`, default on; a warning if the device has no graphics-queue timestamps). Read it next to "Guest pacing": a frame rate below 60 with GPU busy near 100 % is GPU-bound, with GPU busy low it is CPU- or sync-bound. Title screen, 720p: RTX 5070 Ti 5-13 % (0.8-2.2 ms/frame), Radeon iGPU 54 % (9 ms/frame).
- Confirmation avec les timestamps GPU de (Rendu) (`GPU time: busy ...`), iGPU AMD de ce PC, chemin hôte, match :
  GPU occupé 81-91 % du temps, 54-63 ms de GPU par image présentée (~15 fps). Le jeu est donc bien limité par le GPU
  sur iGPU ; il reste à savoir quelles passes coûtent (resolves, post-traitement, dessins).

## (Rendu) D3D12 backend test (2026-10-27)

- The installed SDK already contains the D3D12 backend (`REXGLUE_USE_D3D12=ON` in the SDK cache, D3D12 objects in `rexgpu-xenos.dll`); `--gpu_backend=d3d12` does select it (log: `DXGI adapter: ...`). No separate build was needed. `--d3d12_adapter=1` picks the AMD iGPU.
- Runs of ~2 min from the title to the attract-mode demo match, one instance at a time:
  - RTX 5070 Ti, D3D12, current config (`readback_resolve = "full"`): `DEVICE_HUNG` (TDR) about 70 s after launch, when the demo match loads. Same as the October 2026 note on Intel.
  - AMD iGPU, D3D12: same TDR, with or without `readback_resolve`.
  - RTX, D3D12, `render_target_path_d3d12=rov`: multi-second stalls, then the same TDR.
  - RTX, D3D12, `--readback_resolve=none`: no TDR, 60 fps, but `nvidia-smi` showed ~1 % GPU load at 1440p (Vulkan, same scene: 68 %, 12 ms GPU time per frame). D3D12 was very likely not drawing the 3D scene, so this is no performance result.
  - Reference on the AMD iGPU, Vulkan (auto = host path), demo match 720p: 42-43 fps, GPU busy 98 %, 23 ms per frame (GPU-bound).
- Vulkan-only parts of our port with no D3D12 equivalent: `readback-resolve` (format filter, repeat limit, shape skip; D3D12 only has the plain `d3d12_readback_resolve` switch, so Superstar Threads would need it ported), `render-path-log` / "auto" (D3D12 has its own default: host RTV, ROV on Intel), `vulkan-gpu-time`, `vulkan-pacing-stats` and the local frame counters, `pipeline-cache`, `descriptor-reuse`, the mailbox present option, and the launcher's vertical sync / "rendu précis" options. Shared code that does apply to both: fuzzy alpha (DXBC has it), `resolution_scale`, `empty-resolve`, texture fetch checks, register-write work.
- Conclusion: D3D12 does not help weak or old-driver GPUs today: it hangs on both vendors in the first 3D scene, and the one configuration that survives does not render it. Making it usable means debugging the TDR in the Xenos D3D12 backend, then porting the readback and our Vulkan fixes: work on a renderer we would replace anyway if the native renderer goes ahead (see `NATIVE_PORT_PLAN.md`, feasibility section of 2026-10-27). Not recommended.

## Coût GPU par passe sur l'iGPU AMD (2026-10-27, Perf)

- `rexglue-sdk-vulkan-gpu-time2-passes.patch` (après `vulkan-gpu-time`) : option `vulkan_gpu_pass_profile=true` (désactivée
  par défaut, ralentit le GPU) ; un timestamp est écrit à chaque changement de type de travail enregistré (scène,
  profondeur seule, rectangles = passes plein écran, resolves) et la ligne « GPU passes per presented frame » est
  journalisée toutes les 5 s. Les timestamps vident le pipeline : les temps sont gonflés, seules les proportions comptent.
- Mesure : iGPU AMD de ce PC, chemin hôte (`--vulkan_device=1 --render_target_path_vulkan=rtv`), match 1v1, ~15 fps :
  par image présentée, scène 28 ms (~1 900-2 700 dessins), rectangles 8,3 ms (23 passes plein écran, ~0,36 ms
  chacune : post-traitement), resolves 7,0 ms (60-78 copies de cibles, ~0,1 ms chacune), profondeur seule ~0,1 ms
  (2 112 dessins sans écriture couleur, quasi gratuits). Total ~44 ms contre ~54 ms sans le profil.
- Lecture : ~65 % scène, ~19 % post-traitement, ~16 % resolves. Rien n'est « gaspillé » de façon évidente :
  supprimer des resolves ou des passes plein écran changerait l'image. Leviers sans changer l'image : anisotropie 0
  (+~12 % d'images, déjà dans le préréglage Performance du launcher, avec chemin hôte et sans AA) ; le reste demande de
  réduire le nombre de dessins de scène côté jeu (foule, ring), ce qui n'est pas faisable proprement.
- Piège corrigé : `MarkGpuPass` doit ouvrir la soumission avant d'enregistrer (le tampon de commandes différé est
  remis à zéro à l'ouverture, ce qui effaçait la première marque).

## 2026-10-27 (Perf) — Native renderer: CPU hot paths read from source (no game run)

Read on request of (Main) while the PC was taken by (Rendu): `port/src/reblue/gpu/{hooks/draw,draw,constant_buffers,
pipeline/pipeline_cache}.cpp`, `port/src/native/svr_geometry.cpp`. Nothing was run or built; every point below is a
candidate to measure, not a measured gain.

- **Per draw**, on the render thread, under the single `state().mutex`: `SvrBindDrawGeometry` (one `std::map` lookup
  per used stream and for the index buffer, plus a hash of each buffer's guest bytes once per frame: dynamic
  buffers in full, big static ones by 32 x 64-byte samples), `FlushRenderStateLocked` (pipeline-state hash
  `XXH3` of the whole `PipelineState`, then an `unordered_map` lookup only when a dirty bit is set), then the
  constants.
- **Constants are the most expensive item on a small CPU.** In the native build `vertexShaderConstants` and
  `pixelShaderConstants` are forced dirty at every draw (inline XDK setters are not hooked), so each draw copies and
  byte-swaps two 4 KiB blocks into write-combined memory, plus rebuilds the shared block (32 texture slots, sampler
  `memcmp`s). `ReuseBlock` (skip the copy when the guest bytes are identical to the previous draw's) exists and is on
  for Android, where it was 44 % of the render thread; on PC it was a `constexpr false` ("until measured").
  **Change made (default unchanged):** it is now the cvar `svr_reuse_constant_blocks` (false on PC, true on Android)
  so it can be A/B-tested without a rebuild; the `[perf] cpu per frame` line already prints "constant blocks N
  uploaded, M reused". Cost of the check is a 4 KiB `memcmp` per stage per draw; expected to win when consecutive
  draws share a material, to be measured on 2-4 cores before turning it on.
- **Pipelines are built synchronously on the render thread** the first time a state is seen
  (`GetOrCreatePipeline` -> plume `vkCreateGraphicsPipelines`), and plume passes `VK_NULL_HANDLE` as the pipeline
  cache (`plume_vulkan.cpp:1674`): nothing persists between runs, so every launch re-pays all driver compiles as
  hitches, worst on weak CPUs/iGPUs. A persistent `VkPipelineCache` (same idea as `rexglue-sdk-pipeline-cache.patch` for
  the Xenos path) would fix that. It needs a change in plume, which lives outside the repo
  (`C:\Dev\native-deps\plume`, pinned by `setup.ps1`), so it needs a patch file or a decision from (Main) on how plume
  patches are stored. Not done.
- Smaller: the shader-cache lookups are one `unordered_map` find under a mutex at shader creation only (not per
  draw), and shader conversion uses all cores but two (cvar `wwe2k15_shader_threads`): on a 2-core CPU that is zero
  workers, check `ResolveThreads` behaviour on a 2-core affinity before the first-launch test (the log line "Native
  shaders: preparing ... N / 609 done" will show it).
- `wwe2k15_native` threads: `thread_cpu.ps1` already accepts both process names.

### Measurement plan (scripts written, not yet run)
- `port/tools/dev/native_vs_xenos.sh <tag> <xenos|native> <exe dir> <mask|-> [args]`: one demo-match run with
  optional core affinity, prints time to title, fps / worst gap, per-thread CPU and the `[perf]` lines.
- `port/tools/dev/native_matrix.sh [exe dir]`: Xenos vs native x all / 4 / 2 cores x NVIDIA / AMD iGPU, plus a native
  4-core run with `svr_reuse_constant_blocks=true`. About 45 min of the PC; the native demo trigger (render work >= 6 ms
  in a `[perf]` line) is unvalidated: check the first log. Warm both shader caches first.
- Loading time: the same script prints the seconds to the first "Guest pacing" line (title screen); a load-to-match
  comparison still needs `loadsample.sh` adapted to the native exe.

### Native vs Xenos on small CPUs and the AMD iGPU (measured 2026-10-27, Perf)
Demo match, 30 s of per-thread CPU, `port/tools/dev/native_matrix.sh` (+ rerun of 4 rows). NVIDIA = RTX 5070 Ti,
iGPU = AMD Radeon on `--vulkan_device=1` / `--svr_gpu_device`. Cores simulated by process affinity (all = 16 threads,
F = 4, 3 = 2). "gap" = worst frame gap in the last 6 pacing lines. CPU is % of one core, whole process.

| GPU | cores | Xenos (host RT) | Native |
|---|---|---|---|
| NVIDIA | all | 60 fps, gap 18-19 ms, CPU 91 % | 60 fps, gap 20-23 ms, CPU 71 % |
| NVIDIA | 4 | 60 fps, gap 19-34 ms, CPU 107 % | 60 fps, gap 22-24 ms, CPU 97 % |
| NVIDIA | 2 | 58-59 fps, gap 34-36 ms, CPU 113 % | 60 fps, gap 19-22 ms, CPU 102 % |
| iGPU | all | 21-35 fps, gap 83 ms, CPU 37 % | 60 fps, gap 26-31 ms, CPU 74 % |
| iGPU | 4 | 30-39 fps, gap 66-84 ms, CPU 49 % | 60 fps, gap 29-33 ms, CPU 99 % |
| iGPU | 2 | 29-38 fps (last 2 samples were menus), CPU 33 % | 60 fps, gap 28-32 ms, CPU 108 % |

- Native holds 60 fps everywhere, including 2 cores (about one core busy), where Xenos drops to 58 on the dGPU
  (its `GPU Commands` thread alone takes 49 % of a core) and to ~30 on the iGPU. On the iGPU the native exe is
  CPU-bound, not GPU-bound, at 2-4 cores (render thread 30-38 % of a core).
- `svr_reuse_constant_blocks=true` (NVIDIA, 4 cores): 94 % vs 97 % CPU, same 60 fps: within noise, left off on PC.
- Native is not lighter in total CPU on 2-4 cores (97-108 % vs 107-113 % Xenos on the dGPU); it is the fps and the
  frame pacing that are better.
- Time to the demo match from launch: native 55-94 s, Xenos dGPU 78 s (the demo starts after the title idle time, so
  this is not a loading measure). Title screen at 4-6 s of log time for both.
- Caveats: the first native demo trigger (render work >= 6 ms) fired on the title screen; replaced by "geometry hashed
  per frame >= 1 MB" (title 0, match 2+ MB). The Xenos iGPU rows use a 130 s fallback because `draws/frame` never
  reached 3000 there. The Vulkan driver pipeline cache (`plume-pipeline-cache.patch`) wrote `cache\<uuid>.vk.pcache`
  files (3-4 MB per GPU) but plume's messages go to stderr, so no load/stall gain was measured: not yet verified.

## 2026-10-27 (Perf) — Keyboard text entry: first findings (work paused, the user came back to the PC)
- 2K15 imports `XamShowKeyboardUI` and calls it from one place: the XDK wrapper `sub_83389A78` (TU4 address), itself
  called twice from `sub_833715C8`, a big system-dialog dispatcher (3200-byte frame, switch of 52 cases), called from
  one site in `wwe2k15_recomp.126.cpp`. Which screens reach it is not known yet.
- Test run (Xenos exe, `[log.levels] krnl = "debug"` so the SDK's `XamShowKeyboardUI(...)` line shows): CREATIONS >
  SUPERSTAR > NEW > template > OTHER tab (LB = key `1`, RB = key `3`) > ACCEPT asks "No changes ... Entrance Attire,
  finish editing?" (YES/NO, default NO; the left stick key `A` held 130 ms did not move the choice, try a longer hold
  or `--keybind_dpad_left`). The name screen was not reached before the test stopped, and no keyboard line was logged
  up to there. WWE '12's generated code is not on this PC, so its grid functions cannot be matched by instruction
  similarity; they have to be found in 2K15 itself (PAGE_GUARD trap on a text widget, as in WWE '12's notes).
- `port/tools/dev/nav.ps1` now also knows the keys `1`, `3`, `I`, `K`, `J`.

### Keyboard text entry: test plan (drafted offline, 2026-10-27, Perf)
Skeleton: `port/src/text_entry_kbd.cpp` (not in CMake yet; `Wwe2k15KbdInit(window())` to be called from
`Wwe2k15App::OnPreLaunchModule`; hooks are TODO). Reference: WWE '12 PC `port/src/native/wwe12_kbd.cpp` (module) and
`wwe12_kbdscan.cpp` (dev probe: F6 = scan guest memory for the typed marker text, F5 = PAGE_GUARD radar that logs the
RIP of every write on the buffers' pages).

1. **Slot** (about 30 min, nothing else running, user not on the PC). Close other games; `tasklist`.
2. **Launch** the Xenos exe with the pad-like keybinds so dialogs can be answered:
   `wwe2k15.exe --game_data_root=C:/Dev/games/WWE2K15-TU4 --mnk_mode=true --keybind_dpad_up=Up --keybind_dpad_down=Down
   --keybind_dpad_left=Left --keybind_dpad_right=Right` and in the exe-side `wwe2k15.toml` add
   `[log.levels]` / `krnl = "debug"` (restore the file afterwards: the build copies it back anyway). The stick key `A`
   held 130 ms did not move the YES/NO choice of the "finish editing?" dialog: use `LEFT`/`RIGHT` (D-pad) there.
3. **Key sequence** (`nav.ps1 -keys`, each line then a `-shot`): title `ENTER:500,WAIT8` (twice more `SPACE:300,WAIT10`
   for the two ATTENTION screens) > main menu `S:150` x4 to CREATIONS > `SPACE:300,WAIT2` (submenu) > `SPACE:300,WAIT8`
   (SUPERSTAR > NEW) > `SPACE:300,WAIT8` (template list) > `SPACE:300,WAIT15` (editor, HEAD tab) > `1:150` (LB, wraps
   to the OTHER tab) > `S:130` x7 (ACCEPT) > `SPACE:300,WAIT3` (dialog, default NO) > `LEFT:200` (YES) > `SPACE:300,WAIT10`.
   Expected: the name entry. If the game goes back to the editor, check the shot taken at 1-2 s (a "name"/"info"
   screen may flash) and try `EDIT ENTRANCE ATTIRE` first, or Create a Title (CREATIONS > CHAMPIONSHIP > NEW), or
   Create a Story text, which are the screens the user wants.
4. **Decide case A or B** from the log: any `XamShowKeyboardUI(` line (SDK, krnl debug) while the name screen shows =
   case A (the SDK's ImGui dialog; check that the physical keyboard types in it, `SetTextInputActive`). No line and an
   on-screen letter grid = case B.
5. **Case B, find the functions** (same method as WWE '12): (a) on the grid, type a known marker with the pad (e.g.
   "ABC"); (b) scan guest memory for the marker as ASCII and UTF-16 (port `wwe12_kbdscan.cpp` F6 into a dev-only
   `wwe2k15_kbdscan.cpp` guarded by an env var at configure time; the guest base is `REX_KERNEL_MEMORY()`); (c) set
   PAGE_GUARD on the pages holding the hits and log the RIP of each write while typing one more letter (F5); (d)
   symbolize the RIPs: `llvm-symbolizer --obj wwe2k15.exe` at 0x140000000 + offset with `wwe2k15.pdb` (build dir) ->
   function names `sub_8xxxxxxx` (TU4 addresses); the writer called from the "insert" path is the memset/memcpy pair,
   its caller is the insert function; (e) the widget draw function is the one that reads the line text every frame:
   hook candidates by instruction similarity against WWE '12's `sub_824C9228` are not possible (no WWE '12 generated
   code here), so take the function that draws a text widget and reads the same string, found with a read trap on the
   line page (PAGE_GUARD also fires on reads) and its caller chain with `stackscan.ps1`.
6. **Probes to log** (`wwe2k15_kbd_log`): editor address, insert text and caller LR, BACK caller, screen/widget names
   (widget name offset +0x60 in WWE '12; check in 2K15), the grid's line count limits.
7. **Implement** the TODOs in the skeleton, add `src/text_entry_kbd.cpp` to both executables in `CMakeLists.txt`, call
   `Wwe2k15KbdInit`, build, test in a name field and in Create a Story; keep `wwe2k15_kbd = true` default only if
   case A/B works; with a pad connected nothing changes (keys only go to the text while a text screen is seen).

## (Mods) Create a Story leftovers in 2K15 TU4? (2026-10-27)

Read-only look at `C:\Dev\games\WWE2K15-TU4`; nothing copied into the repo, no game launched.

**Found (readable, uncompressed data only):**
- `pac\menu\menu.pac` (EPAC, 38 MB, not compressed) holds a table of statistics counters. One of them is
  `BASIC_CREATE_STORY_COUNT`, next to `BASIC_CREATE_SUPERSTAR_CAS_COUNT`, `..._DIVA_CAS_COUNT`, `..._ENTRANCE_COUNT`,
  `..._FINISHER_COUNT`, `..._ARENA_COUNT`, `..._CHAMPIONSHIP_COUNT`. That is a usage counter name inherited from 2K14,
  not a feature. `pac\_startup.pac` contains the word `STORY` twice (context not read, file is EPAC).
- No file, folder or `plist360.def` entry (list of every pac) has "story", "scenario", "storyline", "rival" or "feud"
  in its name. The only create-related pacs are `createmode.pac` (55 KB), `menu\CAE.pac` (create an entrance, 3.3 MB),
  `menu\sas.pac` (11 KB), `edit\ed*.pac`, `edit\casThumb*.pac` (CAS = create a superstar). UTF-16 text search in all
  uncompressed pacs (`string.pac`, `string_pf.pac`, `menu.pac`, ...) found nothing about stories/scenarios.

**Not checked (cannot be, yet):** 337 of the 404 pacs (including `createmode.pac`, `sas.pac`, `CAE.pac`, `evt\*`) and
`default.xex`/`default.xexp` are compressed (XCTD `0x0FF512ED` for pacs, XEX2 LZX for the exe), so a strings search
over them says nothing. The generated C++ has no string data (only `sub_8xxxxxxx` code), so engine functions cannot be
matched to strings from it. Missing tools: an XCTD decoder (framing after the 8-byte `0FF512ED 01000000` header is not
documented; first attempt to read it by eye failed) and an XEX unpacker (the SDK only does it in the loader).
Ways to get there: (a) dump the decompressed image and the pacs from guest memory while the game runs (needs the PC;
`memtrap.cpp` in `port/tools/dev` is a starting point); (b) reverse the game's own XCTD routine and write a Python/C
decoder.

**Estimate (honest, from the little we know):** the strongest evidence is negative. 2K15's own menus have no story
entry and the only trace is a stats counter name, so the scenes, texts and save format of 2K14's Create a Story are
most likely absent, not just hidden. Re-enabling it would mean rebuilding the mode (menu scenes, editor logic, save
format, localisation) from scratch against recompiled code: months of work, not a toggle. Cheaper alternatives: none
known. Next step if wanted: item (a) above, to confirm that no editor scenes exist in `createmode.pac` / `menu*.pac`.

### Keyboard text entry: done on the Xenos exe (2026-10-27, Perf, `port/src/text_entry_kbd.cpp`)
- 2K15 does not call `XamShowKeyboardUI` for its text fields (CAW name, Roster Application Form: entrance, abbreviated, ring announcer, commentary, hometown names...): it has its own on-screen letter grid, like WWE '12. (There is no Create a Story in 2K15; earlier notes saying so are wrong.)
- Found without WWE '12's code, with `port/tools/dev/memtrap.cpp` (new dev tool, compile with clang-cl: `scan <pid> <text>` finds a typed marker in the game's memory, `trap <pid> <s> <hex host addr>` attaches as a debugger, sets PAGE_GUARD on the page and logs RIP + return addresses of every read/write; symbolize RIPs with `llvm-symbolizer --obj wwe2k15.exe` at `rip - image base + 0x140000000`; the guest memory is mapped at host 0x100000000 + guest address, plus aliases). Game memory is also readable from Python with ReadProcessMemory (guest = host - 0x100000000, big-endian).
- TU4 functions: insert `sub_828A2CA8(editor, const char* s, 1, 0)`; button handler `sub_828A6478(editor, code)` (code 7 = letter cell, 14 = BACK); text widget draw `sub_82333D98(screen, widget)` every frame. The editor is `screen - 0x770` (editor A4BD5440, screen A4BD5BB0); its line text pointers are at `editor+0xECC` (8 of them), and a grid line is the widget whose `[widget+0xA4]` equals one of them; `[editor+44]` != 0 and cursor `[editor+3460]` are sanity checks. The first typed letter replaces the default text (the game's own behaviour, the pad does the same).
- The module (both exes, cvar `wwe2k15_kbd` default true, `wwe2k15_kbd_log` probe) queues `OnKeyChar` text (z-1000 window listener: AZERTY, Shift, AltGr, repeat; accents become the plain letter), keeps the typing keys away from `mnk_mode` while the grid is up, and replays the queue from the draw hook on the game thread through the two functions above. Tested by the user on the Xenos exe in the Roster Application Form name field; the native exe builds the same file but is not tested yet.
- Test path (nav.ps1): title ENTER, two SPACE (attention screens), `S` x4 = CREATIONS, SPACE x4 (submenu, SUPERSTAR > NEW, template, editor), `1` (LB) = OTHER tab, `S` x7 = ACCEPT, SPACE, `LEFT` = YES (needs `--keybind_dpad_*` keys), SPACE = the Roster Application Form; SPACE on a field opens the grid. The game window must be in front: with the user at the PC, ask them to click it first (Windows refuses a background `SetForegroundWindow`).

### Update 2026-10-27: compressed pacs decoded, Story Designer leftovers found (Mods)

Decoder: WWE '12's `port/src/modmaker/svrfmt/xcompress.cpp` (XMemCompress `0x0FF512ED`: header, chunk-size table,
32 KB LZX frames, libmspack `lzxd`) built as a tiny command-line tool outside the repo (`C:\Dev\work_2k15_unpacked\_tool`,
`xdec.exe in out`). All 337 compressed pacs of TU4 + TU decode without error into `C:\Dev\work_2k15_unpacked`
(3.7 GB, not in Git); the results are `EPK8` containers (the uncompressed ones are `EPAC`). The earlier "cannot read
compressed data" limit above is lifted for pacs; `default.xex` is still packed (no unpacker yet).

What the full search (ASCII + UTF-16, all pacs) shows:
- **`pac\menu\sas.pac` (decoded 106 KB) is a table of 159 "Story Designer" commentary cue names**
  (`SVR11_Cole_StoryDesigner_Rivalry_Match_563`, `..._Made_Cage_Match`, `..._Old_Rivals_Match`, `..._Rematch_Match`,
  `..._Best_of_Series_*`, `..._Brand_War_Match`, `..._Surprised_Cutscene`, ...; Cole/King lines). That is SmackDown vs.
  Raw 2011's Story Designer commentary, still shipped.
- **`sound\Game_Sound_M.pck` holds the audio**: 202 WAV clips with cue labels `SVR11_*_StoryDesigner_*` (Cole/King).
  `Comm.pck` and `Game_Sound_E_S.pck` have none.
- `pac\menu\menu.pac` stats counter `BASIC_CREATE_STORY_COUNT` (see above); `MnUIHD/SD.pac` has a widget label
  "New Story"-ish next to the match-type list (text truncated; not confirmed as a story entry). `com.pac` has an AI
  parameter `DefRivals`; `evt\Winning01.pac` a camera named "Story Audience Camera" (Universe/Winning event).
- No story editor scenes, story text, feud/storyline data or story save format found anywhere in the decoded pacs
  (`createmode.pac` is only the create-mode UI; names `feud`/`rival` elsewhere are Universe-mode rival tron/appear
  events and random bytes).
- Engine side: still not checked (needs `default.xex` unpacked: the SDK loader unpacks it in memory; a standalone
  extractor or a guest-memory dump is the next step, then look for xrefs to the strings `StoryDesigner` / `sas.pac`).

Revised estimate: the 2011/2K14-era Story Designer **commentary audio** and cue list survive, which is the least
costly part of the mode; the editor UI, story data model, scenes and save format do not appear in the data. Bringing
the mode back therefore still means rebuilding the editor, but the commentary could be reused. Unknown until the exe
is searched: whether story-related code is still linked.

- 2026-10-27, native exe tested (same module): typing, Shift/uppercase, accents (e-acute becomes E, c-cedilla becomes C: the game font is upper case ASCII), long names (stops at the limit), backspace and a second field (abbreviated name) all work; AltGr and the Xenos exe with the final backspace path were not re-tested. BACK now calls the grid's delete `sub_828A3670(editor, 1)` directly: the button path (`sub_828A6478` code 14 -> `sub_828A4910`) runs a short state machine and can drop presses that arrive while it runs. `nav.ps1` gained `TYPE:text` (SendKeys; `~e ~a ~c` = accented letters) and the key names `1 3 I K J`.

## 2026-10-28 (Perf) — Issue #11: crash after a Money in the Bank ladder match (Universe, native exe)
- Crash report (reporter's `crash_2026-10-27_19-19-37.txt`, v0.2.0-beta symbols' `wwe2k15_native.map`): access violation reading 0x10000001C (guest base + 0x1C, a NULL guest pointer) at `wwe2k15_native.exe+0x1AA6163`, inside the recompiled `sub_8253D490` (TU4). The function takes an object in r3, follows `[r3+28]`, `[r3+32]`, `[r3+36]` and `[[r3+2712]+28]` untested, then calls `sub_8253CF30`; it has about 10 call sites and returns nothing the callers use.
- Fix: `port/src/crash_guards.cpp` (both exes, cvar `wwe2k15_crash_guards`, default true) hooks `sub_8253D490`, checks those pointers and skips the call when one is NULL (logs "crash guard: sub_8253D490 skipped ..." for the first 5). The crash is in code shared with `wwe2k15.exe`, so the Xenos exe is protected too. Not reproduced (needs a Universe Money in the Bank match); the build boots to the title screen. If the log line appears for a tester, the skipped call might leave the match state half updated: ask for the log.
- Symbolizing a crash from a release: `crash_*.txt` gives `exe+RVA`; with the release's `.map` the function is the last public symbol at or below `0x140000000 + RVA`; for generated functions (`sub_8xxxxxxx`) read `port/generated/default/wwe2k15_recomp.*.cpp` (the PPC mnemonics are in comments).

## 2026-10-28 (Perf) — Issue #9: post-match replay (free space reported to the game)
- 2K15 has the same replay file name as WWE '12 (`Spectacular...rps` string in the game's data) and the same cause is likely: the SDK's host-folder device reported a fixed 64 MiB free (`HostPathDevice`: 128K units x 1 sector x 512 B), and the game wants about 150 MB free on `cache:` before it creates its replay file; below that it turns the highlights/replay off. Evidence: `Documents\wwe2k15\xbox_cache` only held `CAR.cac` and `EditEx.cac` after many matches.
- Fix: `port/patches/rexglue-sdk-freespace.patch` (taken from WWE '12 PC, same SDK commit, applies cleanly): the device reports the host disk's real size and free space. SDK rebuilt and installed, both exes relinked. Result on the first launch with it: `xbox_cache\Spectacular.rps` (157 286 400 bytes) is created at startup next to the two .cac files.
- Not verified in a played match: after a match, MATCH HIGHLIGHTS must be selectable and the replay must play (Xenos and native). To check by hand (a full match is needed; the replay file only proves the game now accepts the volume).
