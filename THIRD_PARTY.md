# Third-party components

This project's own code is under the **GPL-3.0** (see `LICENSE`), like WWE '12 PC
(https://github.com/iMrBurn/wwe12-pc), whose native renderer it reuses. The components below keep
their own licenses. No game data (XEX, disc files, extracted or converted shaders, generated code)
is included in this repository or in its releases.

| Component | Use | License |
|---|---|---|
| [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) | static recompilation, runtime, Xenos GPU emulation | BSD 3-Clause (derived from [Xenia](https://xenia.jp)) |
| [re:Blue](https://github.com/zolaware/reblue) | core of the native renderer (`port/src/reblue`) | BSD 3-Clause (`port/src/reblue/LICENSE.reblue`) |
| [plume](https://github.com/zolaware/plume) | Vulkan layer of the native renderer (built from `native-deps`, not vendored) | MIT |
| [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) (re:Blue fork) | Xbox 360 shader conversion to HLSL (offline tool, not vendored) | MIT |
| [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | HLSL to SPIR-V / DXIL (shipped with XenosRecomp) | MIT (LLVM: Apache 2.0 with LLVM exception) |
| zstd (`port/third_party/reblue_thirdparty/zstd`) | shader cache decompression | BSD 3-Clause / GPL-2.0 dual |
| Licenses in `port/third_party/licenses` | disruptorplus, renderdoc_app (re:Blue dependencies) | see the files |

Projects the native renderer comes from:

- WWE '12 PC by iMrBurn (GPL-3.0): `port/src/reblue` and `port/src/native` (svr_* glue) are its
  native renderer, with WWE 2K15 changes (D3D addresses, device layout, Resolve entry point).
- nefariousjosiah's SvR 2008/2009/2010 ports (GPL-3.0): the native renderer WWE '12 PC is based on.
