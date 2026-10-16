# WWE 2K15 native-renderer plan

## Target architecture

Follow the proven shape of [Josiah's SvR 2008–2010 ports](https://github.com/nefariousjosiah) and the local WWE '12 P project:

- ReXGlue statically recompiles the Xbox 360 PowerPC game code for Windows x64.
- ReXGlue supplies the recompiled-code runtime and the Xbox system services the game still calls.
- A title-specific renderer intercepts the game's XDK Direct3D calls and draws through a native Windows graphics API. WWE '12 P uses `plume` with Vulkan and hooks the same calls that its Xenos D3D census tracks.
- Game shaders are extracted from the user's own game files, converted with XenosRecomp tooling, and loaded as native host pipelines. The renderer also needs resource, vertex/index format, render-state, and presentation mappings specific to WWE 2K15.
- During development the Xenos GPU plugin can remain available as a fallback and comparison path. The goal is that normal gameplay rendering uses the native renderer.

This is not a full engine rewrite and does not mean removing every Xbox compatibility service. It is a static recompilation plus native-rendering port, as in the reference projects. The 2K15 game logic remains the recompiled original code; the work is to make its services, assets, and graphics calls behave correctly on Windows.

## References and reuse

- [SVR 2008 RECOMP](https://github.com/nefariousjosiah/WWE-SVR2008-RECOMP)
- [SVR 2009 RECOMP](https://github.com/nefariousjosiah/WWE-SVR2009-RECOMP)
- [SVR 2010 RECOMP](https://github.com/nefariousjosiah/WWE-SVR2010-RECOMP)
- Local `wwe12-reference/port/cmake/native_renderer.cmake`: native renderer build wiring, shader compilation, plume integration, and static linking.
- Local `wwe12-reference/port/src/native/`: WWE '12-specific D3D hooks, resource and geometry mapping, frame pacing, and title patches.
- Local `wwe12-reference/docs/d3d_functions.md`: observed D3D entry points and argument mapping methodology.

These references provide architecture and investigation methods, not drop-in 2K15 addresses or guaranteed-compatible code. Check each source file's license and preserve required notices before reusing implementation. Keep game code and copyrighted assets out of this repository.

## Phased work

1. **Keep the current runnable baseline.** Preserve the ReXGlue build and its known logs/configuration as a fallback. Record whether each issue belongs to code generation, Xbox service emulation, game-data loading, or graphics.
2. **Map WWE 2K15's D3D surface.** Use XEX symbols/imports and a D3D call census, following the WWE '12 census workflow. Produce a list of called functions and their addresses/signatures; map draw, resource, shader, state, and presentation calls first. Do not copy WWE '12 function addresses.
3. **Characterize shaders and assets.** Inventory shader blobs and PAC variants from the local game dump. Validate extraction and conversion on small samples; keep only tools and metadata in Git.
4. **Build the renderer shell.** Integrate the selected host backend and hook infrastructure into this CMake target. Initially implement device/swapchain and clear/present, then buffers, textures, samplers, vertex declarations, and draw calls. Keep the Xenos path switchable during bring-up.
5. **Reach a visible vertical slice.** Compare menus and a simple arena scene against the existing baseline. Log unsupported calls and resource formats; add focused mappings until menus, entrances, and matches render without Xenos drawing.
6. **Stabilize gameplay and distribution.** Address frame pacing, video playback, input, saves, resolution, shader cache, device compatibility, packaging, and crash logs. Preserve the user's own game files as external inputs.

## Current 2K15 evidence

The local dump inventory records 674 files (about 8.2 GB): 370 `.pac`, 144 `.bk2`, 115 `.xzp`, 25 `.xex`, three `.pck`, plus `.arc`, `.def`, and other files. The main `default.xex` is 24,133,632 bytes. Header scanning distinguishes XCompress/LZX streams, EPAC, EPK8, PACH, Bink 2 (`KB2i`), and AKPK sound banks. These signatures and PAC index bounds are preliminary format observations, not complete decoders.

`native/tools/inspect_game_data.py` performs metadata-only inventory and bounded index checks. See [PORTING_PLAN.md](PORTING_PLAN.md) for the earlier capture and commands. The immediate renderer dependency is a reliable shader/call inventory; archive decoding can progress alongside it, but does not replace D3D mapping.

## Practical boundary

The initial goal is a native-renderer ReXGlue port, consistent with the referenced projects. It does not promise that the current prototype's allocator, video, or frame-rate issues disappear automatically: each subsystem needs a measured fix. If the long-term goal later changes to a standalone engine with no Xbox runtime services, that would be a separate, much larger project.
