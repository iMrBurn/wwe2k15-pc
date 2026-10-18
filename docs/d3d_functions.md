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
