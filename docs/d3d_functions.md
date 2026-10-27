# WWE 2K15 XDK Direct3D call map

This is a title-specific map for the Xbox 360 D3D library linked into WWE 2K15's `default.xex`. It follows the census and hook-identification workflow documented in the local WWE '12 PC project. Addresses must be confirmed against this title's own generated code and runtime captures; WWE '12 and SvR addresses are not interchangeable.

## Method

1. Start from guest functions that call graphics-kernel imports such as `VdSwap`.
2. Census registered functions around that anchor in menu, entrance, and match scenes with `port/tools/gen_d3d_census.py` (`-DWWE2K15_D3D_CENSUS=ON`).
3. Identify methods from their call counts, register arguments, device pointer consistency, and guest D3D object types.
4. Add one hook at a time and compare with the Xenos-plugin baseline before implementing the corresponding native Vulkan behavior.

## Confirmed code anchor

| Likely XDK method | Guest function | Evidence | Status |
|---|---|---|---|
| `D3DDevice_Swap` / present submission | `sub_82BA5FA8` (`0x82BA5FA8`) | Calls the imported `VdGetSystemCommandBuffer`, `VdSwap`, and `VdPersistDisplay` functions. It is the only generated guest function calling `VdSwap`. | Strong candidate; runtime call cadence still to be captured. |

## Unmapped

Clear, resolve, render-target, viewport/scissor, texture, stream/index buffer, shader, and draw calls remain unidentified. Do not infer WWE 2K15 addresses from the WWE '12 map. The initial census range `0x82BA0000–0x82BB0000` is a candidate around the present anchor, not a proven full-library boundary.

## Identified functions (2026-10-27, TU4 code)

All 22 D3D functions that re:Blue's renderer hooks by name are identified in
`port/src/native/wwe2k15_d3d_names.h`. Method: `port/tools/match_d3d_functions.py` compares the
PowerPC instruction sequences of the 564 generated functions in 0x82B9C000-0x82BB8000 (the window
around the only VdSwap caller, `sub_82BADF20`) with the 22 functions the SvR 2010 port names;
17 match at 1.00, the rest at 0.83-0.97 (Swap, Resolve, Present, SetRenderTarget, Begin/
DrawIndexedVertices, SynchronizeToPresentationInterval, which are longer in this newer XDK). An
in-game census (`-DWWE2K15_D3D_CENSUS=ON`, now on the TU4 window) confirms them: every candidate
takes the device pointer in r3 and the call rates fit (DrawIndexedVertices about 67,000 calls/s in
a 3D scene, SetTexture ~39,000/s, Resolve and SetRenderTarget a few per frame). Unlike WWE '12, 2K15
does call BeginVertices/EndVertices and CreateVertexShader/CreatePixelShader/CreateVertexDeclaration
(290 vertex and 399 pixel shaders created by the title screen), so the matching re:Blue hooks can stay
on. `Present` was never called. The `D3DDevice_Swap` address is also hooked by `framerate_hook.cpp`;
the native build will have to merge the two hooks.
