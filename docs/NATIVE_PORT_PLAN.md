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
- 2026-10-27, stage 2 done: the shaders are compressed in 2K15's archives (no raw 0x102A11xx
  container in the files), so they are dumped when the game creates them (`port/src/shader_dump.cpp`,
  cvar `wwe2k15_dump_shaders`, off): 609 containers (228 vs, 381 ps), all created at boot, converted
  by reblue-XenosRecomp with 0 errors into the ignored `port/generated/native/shader_cache.cpp`.
- 2026-10-27, stage 3 (shell) done, and the first 3D scene draws: `wwe2k15_native.exe`
  (`-DWWE2K15_NATIVE_RENDERER=ON`, `port/cmake/native_renderer.cmake`) shows the title screen and the
  attract-mode demo match (WrestleMania arena, crowd, wrestlers) at a steady 60 fps on the RTX 5070 Ti
  (work 11.7 ms/frame, no hitch over 25 ms in 600 frames). What it took:
  - SDK: `rexglue-sdk-null-gpu.patch` (the null guest GPU from WWE '12 PC: the game's command stream
    keeps moving, nothing is drawn by the Xenos emulation).
  - Code from WWE '12 PC: `port/src/reblue` (re:Blue + its changes), `port/src/native` svr_* glue,
    `port/third_party`; WWE '12-only features stubbed in `native/wwe2k15_native_stubs.cpp`.
  - `D3DDevice_Resolve` is the public `sub_82BAA2E8`: its only callee `sub_82BA9190` (the 0.83
    instruction match) takes two extra arguments, and hooking it read guest address 0x1C.
  - The D3D device's private block (vertex declaration, index buffer, render targets, streams,
    viewport, scissor, shaders) is +0x18 further than in WWE '12 (`reblue/gpu/d3d.h`); the register
    shadows did not move.
  - Bring-up switch: cvar `svr_hooks_off` (comma list of hook names, or `all`) skips the renderer's
    side of those hooks while the game's D3D functions still run.
  - Not yet: the frame pacing log (framerate_hook's Swap hook is left out of the native exe), the
    launcher option, menus/entrances/match checks, Superstar Threads readback with 2K15's texture
    layout, the viewport offset (+0x32E0, inferred), texture packs and the other WWE '12 extras.
  - Licensing: the repository now carries `LICENSE` (GPL-3.0) and `THIRD_PARTY.md`, as WWE '12 PC does.
- 2026-10-27, stage 4 first pass: menus, character select and a real 1v1 match (Cena vs Reigns,
  `ab3.sh` with `EXE=wwe2k15_native.exe`) render correctly in `wwe2k15_native.exe`, no guest fault.
  - RTX 5070 Ti, automatic 2x (2560x1440): menus and match 60 presents/s, entrances 30 (the game's
    own entrance rate, as on the Xenos path), CPU 80 % of one core in the match (Xenos path ~100 %).
  - AMD Radeon iGPU (`--svr_gpu_device="AMD Radeon(TM) Graphics"`, automatic 720p on integrated
    GPUs): match 60 presents/s, entrances 30, CPU 80 %. The Xenos path on the same iGPU and match:
    14-18 fps (host render targets) or 3-6 fps (FSI). This is the weak-GPU win the native renderer
    was for. `svr_render_scale = 540` (0.75x, from WWE '12 PC) is also available for weaker GPUs.

## Shaders: converted on the player's PC (2026-10-27)

Decision (user, through (Main)): no game-derived data in any package, so `wwe2k15_native.exe`
links no generated shader cache. Flow, all in `port/src/native/wwe2k15_shader_cache.cpp`:

1. When the game creates a shader (`D3DDevice_CreateVertexShader` / `CreatePixelShader`, re:Blue's
   `CreateShader`), the container is hashed (XXH3, as XenosRecomp keys it) and looked up in
   `cache/native_shaders.bin` beside the exe (loaded once, at the first lookup).
2. A container the cache doesn't have is queued for conversion on worker threads (all cores but
   two, cvar `wwe2k15_shader_threads`): XenosRecomp's translator (built into the exe, MIT;
   `port/src/native_xr`) writes HLSL with its `shader_common.h` prelude (embedded at build time),
   `dxcompiler.dll` (loaded at run time from beside the exe) compiles it to SPIR-V with the options
   XenosRecomp uses for Vulkan, smol-v packs it. The result is appended to the cache file and
   handed to every `GuestShader` waiting for that hash.
3. Until a shader is ready its `GuestShader` has no cache entry, and re:Blue skips the draws that
   need it (its existing cache-miss path): nothing crashes, the picture fills in. A shader that
   fails to convert is logged once and its draws stay skipped. Without `dxcompiler.dll` the game
   still runs (black picture) and the log says to reinstall.
4. Measured on the dev PC (16 threads, 14 converting): the 609 shaders the game creates at boot
   are converted in 2 s on the first native launch (log lines `Native shaders: preparing the
   game's shaders, N / 609 done`), cache file 3.0 MB; the next launches load all 609 from it and
   convert nothing. The picture matches the build-time cache it replaces (same pipelines created,
   demo match at 60 fps). A cut-short record (crash while writing) is ignored and converted again;
   a cache from another converter version (tag = shader_common.h hash + version) is rebuilt.

Shipping: `dxcompiler.dll` (17.1 MB, from reblue-XenosRecomp's dxc-bin) beside the exe, plus DXC's
license texts (LICENSE-LLVM.txt, LICENSE-MS.txt: not in that dxc-bin folder, to take from the
matching DirectXShaderCompiler release). `dxil.dll` is not needed (SPIR-V only). The translator
adds about 0.3 MB to the exe. `cache/native_shaders.bin` is per player and never packaged.
The first launch is short enough (seconds, more on 2-4 core PCs) that no separate "prepare"
step is needed; the launcher can still show a note for the first native launch.

## Native checks of 2026-10-27 (afternoon, RTX 5070 Ti)

- Superstar Threads (CREATIONS > SUPERSTAR THREADS, Adam Rose): the attire renders clean from afar
  (`work/dev/v_11.png`) and close up in the editor (`v_15.png`), and a colour change (gauntlet to
  white) is recomposed on the GPU and read back cleanly, no scrambled bands (`v_16.png`). The
  WWE '12 readback path works unchanged: 2K15's texture objects keep the fetch constant at
  +0x1C..+0x30 (checked in 2K15's own SetTexture, sub_82BA32A0).
- Menus, character select, 1v1 match (Cena vs Reigns), crowd, signs, HUD text and the arena
  graphics match the Xenos path in the screenshots (`nab_*.png`, `nent_e*.png`, `nst_*.png`).
- Launcher path: the command line the launcher builds for "Rendu natif" (no gpu_plugin/backend,
  `--svr_render_scale`, `--window_*`, ...) starts `wwe2k15_native.exe` and runs at 60 fps with no
  unknown option; the checkbox itself was not clicked (code read: `launcher.cpp` OptionArgs).
- Full match (user-played, Bo Dallas vs Sami Zayn, Night of Champions), native then Xenos, same
  match settings (`work/dev/natm_*.png`, `xen_*.png`): entrances 30 fps, match 60, victory scene
  and replay 30 then the Rematch/Exit screen, on both paths; no crash, no error in the native log.
- Difference found: the crowd is too bright in native, about 1.8x the Xenos average in the
  WrestleMania attract demo (`dn_04.png` vs `dx_04.png`, top half R/G/B 37/34/46 vs 20/17/30) and
  strongly red in Night of Champions (67/39/36 vs 23/14/15, its red stage light). Ring, wrestlers,
  referee and HUD match. Wrestler shadows on the mat look weaker in native, not yet compared on
  the same camera. Ruled out with a one-off SetTexture log in the demo: no bound texture has a
  fetch-constant exp_adjust or a gamma sign. Also tried (60-min time box, release day): applying
  the vertex-fetch exponent bias in XenosRecomp (`vfetch` expAdjust, which Xenia applies and the
  translator ignores): crowd unchanged (top band 28/30/51 native vs 13/14/31 Xenos,
  `dn4_04.png`), so it was not kept. **Known issue for 0.2.** Next: find the crowd draw (its
  shaders and constants) and compare what it samples on both paths.
- Clean build (2026-10-27): a fresh clone of the repo builds `wwe2k15_native.exe` with
  `port/tools/setup_native_deps.ps1` (plume e0c8871 + `plume-reblue.patch` +
  `plume-pipeline-cache.patch`; reblue-XenosRecomp 339af41 + `xenosrecomp-reblue-wwe12.patch`,
  both base patches copied from WWE '12 PC). A fresh checkout's first configure has no
  `wwe2k15_recomp` target yet: the native exe now skips it until codegen has run, so the order
  is configure, build `wwe2k15_codegen`, configure again, build (checked from a clone).
- FPS counter (`--wwe2k15_show_fps=true`, the launcher option) was missing in native: ReXApp only
  set up the ImGui overlays with a presenter or with no graphics system, and the null GPU is a
  graphics system without a presenter. `rexglue-sdk-null-gpu.patch` now takes the detached path
  in that case too (as WWE '12 PC's SDK patch does); the counter shows at the title screen
  (`work/dev/fps_native.png`). The Xenos exe is unchanged (it has a presenter).
- Not checked yet: an actual AMD/Intel discrete GPU.

### Release-notes paragraph (for 0.2)

**Native renderer (experimental).** Tick "Rendu natif (expérimental)" in the launcher to start
`wwe2k15_native.exe` instead of the normal renderer. It draws the game's graphics directly with
Vulkan instead of emulating the Xbox 360 GPU, which makes weak and integrated GPUs much faster
(on our AMD Radeon iGPU a match goes from 15-40 fps to 60 fps). On the first launch it prepares
the game's shaders on your PC for a few seconds; later launches reuse them. Limits: it needs a
Vulkan 1.2 GPU and driver (if not, a message tells you to update the driver or untick the
option); the crowd looks brighter than with the normal renderer; it has been tested on NVIDIA and
an AMD integrated GPU only. If something looks wrong, untick the option to go back to the normal
renderer, and send us the log.

**Rendu natif (expérimental).** Cochez « Rendu natif (expérimental) » dans le launcher pour lancer
`wwe2k15_native.exe` à la place du rendu habituel. Il dessine les graphismes du jeu directement
avec Vulkan au lieu d'émuler le processeur graphique de la Xbox 360, ce qui rend les cartes
graphiques faibles et intégrées beaucoup plus rapides (sur notre iGPU AMD Radeon, un match passe
de 15-40 à 60 images/s). Au premier lancement, il prépare les shaders du jeu sur votre PC pendant
quelques secondes ; les lancements suivants les réutilisent. Limites : il faut une carte et un
pilote compatibles Vulkan 1.2 (sinon un message demande de mettre à jour le pilote ou de décocher
l'option) ; le public paraît plus clair qu'avec le rendu habituel ; il n'a été testé que sur
NVIDIA et sur un GPU intégré AMD. Si quelque chose s'affiche mal, décochez l'option pour revenir
au rendu habituel et envoyez-nous le journal.
- Robustness (2026-10-27): a GPU/driver without the Vulkan 1.2 features re:Blue needs
  (bufferDeviceAddress, descriptor indexing; device.cpp logs which) now gets a bilingual message
  box telling the player to update the driver or untick "Rendu natif" in the launcher, then the
  exe quits (no black window). A shader cache that can't be written (read-only folder; tested by
  turning `cache` into a file) logs one warning and the game runs normally, converting the
  shaders again at each launch (2 s here).

## 0.2 issues (2026-10-28)

- **#7 brightness stuck / crowd too bright (fixed).** WWE 2K15 uploads a display gamma ramp (its
  brightness option) through DC_LUT registers; the Xenos path applies it at swap, the native
  present pass ignored it. The null guest GPU's shared CommandProcessor still records the
  256-entry table, so `native/wwe2k15_gamma.cpp` reads it each present and fits it as
  scale * x^gamma (default setting: table 32/64/128/192/255 -> 53/162/396/639/884 of 1023, fit
  0.93 * x^1.32); `gamma_correction_ps.hlsl` takes a negative Param1 as that scale. Measured on the
  WrestleMania attract demo: crowd band 14/15/28 native vs 13/14/31 Xenos (was 31/32/46), ring
  floor 150 vs 145. This was also the "crowd too bright" 0.2 known issue. The ramp is re-read every
  frame, so the in-game slider applies at once (not clicked through the options menu here).
- **#10/#12 white attire and hair (AMD; not reproduced).** The reporter's log (Vega 7) shows 64x64
  reflection cubes (slot 11, k_8_8_8_8 tiled) sampled through texture headers other than the one
  the game resolved into, so they were unresolved. `SvrResolveTexture` now falls back to a GPU
  target with the same fetch dwords 1-2 (same memory, format, size): on the AMD iGPU those
  textures now resolve (log "sampled as target ... (same memory)"). On this PC (RTX 5070 Ti and
  the AMD iGPU) 0.2.0 shows no white attire with Cena, Reigns or Orton, so whether this fixes the
  reports is unconfirmed; an affected model (Superstar Threads attire, 2K Showcase or DLC such as
  Randy Orton '11) still has to be tested, and the reporters asked to retry.
- **#8 (Intel, igvk64.dll) and #6 (GeForce 940MX, nvoglv64.dll, Xenos exe) crashes inside the
  Vulkan driver.** The crash reports list every thread but skipped the faulting one, so they show
  no caller. `crash_report.cpp` now writes the faulting thread's own stack (40 return addresses
  from the exception context) first; the native "GPU caps" line now has the driver version and
  VRAM. Needs a new report from the reporters with the next release.
- **GPU robustness (2026-10-28).** Native: every Vulkan GPU is logged at startup ("Vulkan GPU:
  name, type, API, driver, vendor"); when the chosen GPU (svr_gpu_device, else plume's best,
  discrete first) can't be created or lacks bufferDeviceAddress/descriptorIndexing, the others
  are tried in turn ("Using GPU ... instead") before the "Vulkan too old" message (laptops with
  an old discrete GPU beside a newer integrated one, or the reverse). Xenos exe: the "auto"
  render target path now picks host render targets on NVIDIA GPUs older than Pascal (device ID
  below 0x1800 outside 0x15xx-0x16xx: Kepler, Maxwell such as the 940MX of issue #6), in
  `rexglue-sdk-render-path-log.patch`. Checked here: both GPUs listed, RTX 5070 Ti keeps FSI.
- **Anti-aliasing and anisotropy (2026-10-28).** The native renderer draws the game's own render
  targets without MSAA (as in WWE '12 PC; `bd_msaa` only applies to re:Blue's own scene path), so
  AA is FXAA in the present pass: cvar `svr_fxaa` (0 off, 1, 2 strong; classic FXAA in
  `gamma_correction_ps.hlsl`, level carried in the present pass's unused second descriptor
  slot). Anisotropic filtering already works through `bd_anisotropy` (default 16). Launcher
  (asked): in native mode pass `--svr_fxaa=<aa 0/1/2>` and `--bd_anisotropy=<0/2/4/8/16>` from
  the existing AA and anisotropy choices.
- **Parity review from source (no game run, PC busy).** Brightness/crowd now match (0.2.1).
  Shadows: reblue-XenosRecomp rewrites the guest's binary shadow-map taps into bilinear
  filtered compares (`shadowCmp2D`, a WWE '12 change), so contact shadows are softer than the
  Xenos path's point-sampled PCF, which fits the "weaker shadows" seen on 10-27; the ring floor
  in the WrestleMania demo is 147-150 native vs 138-145 Xenos. To confirm on the same camera
  (pause in a match, both paths) before changing it. Post-processing (bloom, depth of field) is
  drawn by the game's own shaders on both paths; no difference seen in the demo captures.
- **Intel crash at match start (#8, #13) and dark wrestlers in pins (#14, #16), 2026-10-29.**
  Main symbolized the Intel reports: plume clearColor <- `Video::ClearTexture` <-
  `SvrResolveTarget`, right after "SvR target ...: 512x512 fmt 25" (k_16_16 -> R16G16_UNORM).
  R16G16_UNORM was not in `IsRenderTargetCapable`, so that texture was created without
  colour-attachment usage, its resolves were skipped ("dst format 27 is not RT-capable", on every
  GPU) and `ClearColorTargetLocked` still cleared it through a framebuffer: NVIDIA and AMD let it
  pass, Intel's driver read a null object. Fixes: R16G16_UNORM is RT-capable when the device
  reports it as a colour attachment (logged at startup); `ClearColorTargetLocked` skips any
  non-RT-capable texture; the EDRAM-only surface formats k_16_16_16_16_EDRAM (0x1A20AB55, a
  512x512 surface that failed to create) and k_16_16_EDRAM map to float formats. That 512x512
  two-channel target is probably the close-up shadow map the pin/grapple cameras use, which would
  explain the dark wrestlers there. Checked on the RTX: a native match runs, no RT-capable or
  0x1A20AB55 errors; a pin was not captured (the run was cut short), and no Intel GPU here.
  No Vulkan validation layer is installed on this PC (it would need the Vulkan SDK).
- **White attire / blank DLC models: draws no longer skipped while their shader converts
  (2026-10-30).** Idea from Jarrod's WWE 2K14 port (v0.2.1: texture-building draws skipped
  during slow shader compiles were never redone). Here a GuestShader with no cache entry yet made
  `GetOrLinkShader` return null and the draw was dropped; the draws that compose attire, skin and
  damage textures run once at load, and DLC / Showcase / Superstar Threads wrestlers bring shaders
  the boot set (609) does not have (reporters' caches reach 1246). Now `GetOrLinkShader` calls
  `Wwe2k15WaitShader` (at most 2 s): that shader's job moves to the front of the queue and the
  draw waits for it. Cold-cache check here: four title draws waited 11-26 ms and rendered (1.6 s
  before the job was moved forward). Not checked: an affected DLC wrestler in a match.
