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
- WWE 2K15's confirmed code anchor and still-unmapped calls are tracked in [d3d_functions.md](d3d_functions.md).

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

### First D3D census anchor

The generated 2K15 code shows `sub_82BA5FA8` calling the imported Xbox display function `VdSwap`. This anchors the initial candidate D3D code window at `0x82BA0000–0x82BB0000`; the window is not yet confirmed to contain the complete D3D library. `port/tools/gen_d3d_census.py` generates optional hooks for registered functions in that range and writes `wwe2k15_d3d_census.csv` beside the executable. Build with `-DWWE2K15_D3D_CENSUS=ON`, then capture menu, entrance, and match scenes separately. The census is diagnostic and off by default because it adds per-call overhead. Expand the address window only after the first capture identifies its active boundary.

## Practical boundary

The initial goal is a native-renderer ReXGlue port, consistent with the referenced projects. It does not promise that the current prototype's allocator, video, or frame-rate issues disappear automatically: each subsystem needs a measured fix. If the long-term goal later changes to a standalone engine with no Xbox runtime services, that would be a separate, much larger project.

## Feasibility and scope (2026-10-27, Rendu)

### What the WWE '12 renderer is (iMrBurn/wwe12-pc, main, read 2026-10-27)

- It is Vulkan, not D3D11: **re:Blue** (zolaware/reblue, BSD-3) on top of **plume** (zolaware/plume, MIT), adapted from nefariousjosiah's SvR 2008-2010 ports (GPL-3.0), which derive from the UnleashedRecomp line. plume's D3D12 path is cut in its build. It is light because it draws the game's D3D calls directly with host render targets: no PM4 parsing, no EDRAM emulation, no per-draw shader translation at run time.
- Hook level: the game's statically linked XDK D3D functions (16 named in `port/src/native/wwe12_d3d_names.h`: Swap, Resolve, Clear, SetRenderTarget, DrawIndexedVertices, Begin/EndVertices, SetTexture, Set{Vertex,Pixel}Shader, SetVertexDeclaration, SetStreamSource, SetIndices, ...), overridden with `REX_HOOK`. Draws read the stream/shader/viewport state that the game left in its guest D3D device object (fixed offsets).
- Shaders: extracted offline from the user's `.pac` files (Yuke's BPE), translated with XenosRecomp (hedge-dev, plus a WWE '12 patch) to HLSL, then DXC to SPIR-V, into a generated, unversioned shader cache keyed by an XXH3 hash of the guest shader container.
- Resolves: alias, copy or shader blit; predicated tiling emulated by growing the surface; GPU-composed attire textures read back to guest memory (the same need as our Superstar Threads readback).
- Textures are untiled and endian-swapped on the CPU; DXT passes through as BC.
- Vertex data are un-swapped in the shader from the declaration.
- Post-processing is the game's own shaders, with some passes skipped by hash.
- Size: re:Blue GPU code ~16-18k lines (generic), plus ~3k lines of WWE '12-specific glue (`native/svr_*`, graphics menu), plus the generated shader cache.
- Known open issues there: coloured blocks on Intel/AMD, white eyes on AMD, car translucency, no Vulkan on old Intel. History before 2026-10-11 is not in Git, so the original build-up time is unknown.

### What carries over to 2K15

- Same family: 2K15 is also a Yuke's title with the XDK D3D library linked statically. In the TU4 code the only `VdSwap` caller is `sub_82BADF20` (base game `sub_82BA5FA8`, see `d3d_functions.md`), so the WWE '12 method applies: census, then identify the ~16 entry points.
- The census window in `gen_d3d_census.py` (0x82BA0000-0x82BB0000) is around the base-game address; with the TU4-patched code it must be moved around 0x82BAxxxx-0x82BBxxxx (TU4 shifts this library by about +0x8000) or computed from the anchor.
- Generic parts (re:Blue/plume device, pipelines, resolve machinery, untiling, XenosRecomp) carry over as code, under their licenses. Their GPL-3.0 parts would make the combined 2K15 renderer GPL-3.0: check that this repository's license allows it before importing.
- Not carried over: every address, device/shader/declaration object offset (2K15 uses a newer XDK than WWE '12), shader hashes, surface-size heuristics, and the shader extraction (2K15's archives: EPAC/EPK8/PACH/XCompress variants, see "Current 2K15 evidence"; whether BPE is used is unknown).
- What we keep from the current port: the recompiled game, kernel services, launcher, mods, saves, and the `rexglue-sdk-*` system patches. GPU-side patches (readback, FSI/auto, pacing stats) become obsolete once the native path draws.

### Staged plan

Each stage is testable against the Xenos baseline; the Xenos path stays as a switch until stage 6.

1. **Census (2-3 days).** Fix the TU4 window, run menu / entrance / match captures, and name the D3D entry points and device offsets. Go/no-go point: if the 2K15 D3D library differs a lot from WWE '12's, re-estimate.
2. **Shader pipeline (1 week).** Extract 2K15 shader containers from the user's files and run XenosRecomp + DXC on them. Count failures and fix the translator patch.
3. **Shell (1 week).** Import re:Blue/plume, add swap / clear / present and the menus' 2D draws: visible menus without Xenos drawing.
4. **3D (2-3 weeks).** Vertex declarations, textures, render targets and resolves, depth, predicated tiling: arena, crowd, wrestlers.
5. **Game features (2-3 weeks).** Superstar Threads / created attire readback, titantron videos (Bink to texture), shadows, post-processing, entrance effects, resolution scaling.
6. **Compatibility (ongoing).** NVIDIA / AMD / Intel, weak GPUs, pipeline cache, crash logs; then make it the default.

Rough total: 2-3 months of focused work before the native path can be the default, on the assumption that the WWE '12 code imports cleanly. The biggest risks are 2K15-specific shaders that XenosRecomp mistranslates and engine features WWE '12 does not use. Nothing has been implemented; this needs the user's go.

## Progress

- 2026-10-27, stage 1 done: the 22 D3D entry points re:Blue hooks are identified (see
  `d3d_functions.md`, `port/src/native/wwe2k15_d3d_names.h`). WWE '12 PC is cloned in
  `C:\Dev\wwe12-pc` (user's go): same SDK commit c94f5eb plus `rexglue-sdk-reblue.patch`, plume
  e0c8871 and reblue-XenosRecomp 339af41 pinned by its `setup.ps1`; GPL-3.0. Decision: the native
  renderer is built as a second executable (`wwe2k15_native.exe`, `SVR_NATIVE_RENDERER`), chosen
  in the launcher, so the Xenos/Vulkan exe stays the default and untouched.
