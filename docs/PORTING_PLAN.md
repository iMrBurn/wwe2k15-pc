# ReXGlue prototype notes (not the native PC port)

## Scope

This file records the experimental Xbox 360 recompilation made with ReXGlue. That build still uses Xbox kernel, memory, and graphics compatibility layers, so it does not satisfy the user's requirement for a native PC game without an emulator. The captured logs are useful as research evidence, but chasing ReXGlue allocation errors is no longer the project goal. See [NATIVE_PORT_PLAN.md](NATIVE_PORT_PLAN.md) for the revised direction.

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
- The latest failure log shows 3,330 `page count too big for requested range` allocations. ReXGlue already documented the matching 4 KiB ranged-allocation workaround for WWE '13. Applied the same title-specific offset handling to WWE 2K15 (Title ID `545408B6`); the patch is kept in `port/patches/rexglue-sdk-wwe2k15-memory-range.patch`. Rebuilt and reinstalled the local SDK, then rebuilt the game executable successfully. The run must be repeated to confirm this removes the allocation failures.
- The latest run (`wwe2k15_023.log`) confirmed a D3D12 crash: `DXGI_ERROR_DEVICE_HUNG` (`0x887A0006`) after about 65 seconds. The invalid-fetch bypass was enabled and produced no fetch warnings, but the GPU hung; reverted it. Added a `gpu_backend` runtime option to ReXApp and select Vulkan for WWE 2K15 to avoid the D3D12 path. The Xenos plugin supports both backends.
- Vulkan run `wwe2k15_024` closed normally after about 2.5 minutes, but warning logs rolled over repeatedly (more than ten 5 MiB files) due invalid texture fetches. Run `wwe2k15_025` with the bypass enabled also closed normally, with no invalid-fetch warnings, but still logged 3,333 `page count too big` allocation failures, 183 unimplemented XCTD compression queries, 12 missing `streaming_install.pck` requests, and three `Resolve region is empty` draw failures. The bypass therefore quieted the texture warnings without fixing the memory failures; because it can reinterpret invalid descriptors, the current build now disables it again while retaining Vulkan. The flicker and slow video need another run to determine whether the bypass affected the displayed artifacts.
- Log `wwe2k15_020.log` confirms the title initializes D3D12 on Intel UHD, then reports repeated physical heap allocation failures and unimplemented `XFileXctdCompressionInformation` queries. It also requests `GAME:\Sound\streaming_install.pck`, which is absent from the supplied game folder. These are separate compatibility/content issues; do not treat the missing streaming pack as confirmed required content.
- The latest captured run, `wwe2k15_020.log`, initialized the Intel D3D12 adapter and closed normally, with no device-loss/fatal errors and no invalid-fetch warnings while the bypass was enabled. It still logged 3,330 physical-allocation range failures (`page count too big`), 3,878 parent-heap allocation failures, and 119 unimplemented XCTD-compression queries. These guest memory allocation failures are the strongest remaining evidence for the in-game instability and low frame rate.
- The completed pass reported seven functions larger than the SDK's 1 MiB code-size threshold: `0x82F2BFF0`, `0x82F39CD0`, `0x82F479B0`, `0x82F55328`, `0x82F624C8`, `0x82F6EE90`, and `0x82F7CB68`. Inspect these before treating generated output as ready to compile.
- `find_fptrs.py` suggested setjmp `0x833AF8F0` and longjmp `0x833AF5D0`. The generated PPC-to-C++ confirms the first saves the nonvolatile register context and the second restores it, so both addresses are enabled in `wwe2k15_config.toml`.

The SDK source confirms that the settings from the WWE 2K14 notes exist in this revision. Defaults are: `async_shader_compilation=true`, `gpu_allow_invalid_fetch_constants=false`, `readback_resolve="none"`, `render_target_path_vulkan=""`, `log_max_file_size_mb=5`, and `log_max_files=20`. These are SDK defaults, not measured game-run values.

The working manifest contains a machine-local path to the user's XEX and is ignored by Git. Start from `port/wwe2k15_manifest.example.toml` and point it at your own game files on another machine. The generated C++ is large and remains ignored.

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
