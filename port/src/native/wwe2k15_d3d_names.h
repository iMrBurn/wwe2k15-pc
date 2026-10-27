// WWE 2K15 (TU4 code) addresses for the XDK D3D functions that re:Blue's renderer hooks by name.
//
// Found on 2026-10-27 by matching the instruction sequences of the 564 functions around the only
// VdSwap caller against the 22 named D3D functions of the SvR 2010 port (svr_d3d_names.h), then
// checked against an in-game call census (port/tools/gen_d3d_census.py: call rates, device pointer
// in r3). The score is the instruction-sequence similarity with the SvR 2010 function.
// Force-included into the native renderer sources, as wwe12_d3d_names.h is in WWE '12 PC.

#pragma once

#define WWE2K15_NATIVE 1

#define D3DDevice_Present                           sub_82BAE688  // 0.83 match; never called in the census
#define D3DDevice_Swap                              sub_82BADF20  // the only VdSwap caller (base sub_82BA5FA8); also hooked by framerate_hook.cpp
#define D3DDevice_SynchronizeToPresentationInterval sub_82BADA60  // 0.97
#define D3DDevice_Resolve                           sub_82BA9190  // 0.83 (2K15's is longer); ~1 call per frame
#define D3DDevice_Clear                             sub_82BAFA18  // 1.00
#define D3DDevice_SetRenderTarget                   sub_82BA6458  // 0.96
#define D3DDevice_SetDepthStencilSurface            sub_82BA67E8  // 1.00
#define D3DDevice_SetScissorRect                    sub_82BA59E0  // 1.00
#define D3DDevice_BeginVertices                     sub_82BABC80  // 0.92; 2K15 does use it (~2,000 calls/s)
#define D3DDevice_EndVertices                       sub_82BABC70  // 1.00
#define D3DDevice_DrawIndexedVertices               sub_82BAC140  // 0.95; the hottest draw (~67,000 calls/s in a scene)
#define D3DVertexBuffer_Unlock                      sub_82BA75B0  // 1.00
#define D3DIndexBuffer_Unlock                       sub_82BA1208  // 1.00
#define D3DDevice_CreateVertexDeclaration           sub_82BAB638  // 1.00
#define D3DDevice_CreateVertexShader                sub_82BABB30  // 1.00 (290 shaders created by the title screen)
#define D3DDevice_CreatePixelShader                 sub_82BAB948  // 1.00 (399 created)
#define D3DDevice_SetTexture                        sub_82BA32A0  // 1.00
#define D3DDevice_SetVertexShader                   sub_82BAB318  // 1.00
#define D3DDevice_SetPixelShader                    sub_82BAB110  // 1.00
#define D3DDevice_SetVertexDeclaration              sub_82BAB530  // 1.00
#define D3DDevice_SetStreamSource                   sub_82BA5B28  // 1.00
#define D3DDevice_SetIndices                        sub_82BA5CD0  // 1.00

// Original bodies (__imp__ = the recompiled function, for hooks that call through).
#define __imp__D3DDevice_Present                           __imp__sub_82BAE688
#define __imp__D3DDevice_Swap                              __imp__sub_82BADF20
#define __imp__D3DDevice_SynchronizeToPresentationInterval __imp__sub_82BADA60
#define __imp__D3DDevice_Resolve                           __imp__sub_82BA9190
#define __imp__D3DDevice_Clear                             __imp__sub_82BAFA18
#define __imp__D3DDevice_SetRenderTarget                   __imp__sub_82BA6458
#define __imp__D3DDevice_SetDepthStencilSurface            __imp__sub_82BA67E8
#define __imp__D3DDevice_SetScissorRect                    __imp__sub_82BA59E0
#define __imp__D3DDevice_BeginVertices                     __imp__sub_82BABC80
#define __imp__D3DDevice_EndVertices                       __imp__sub_82BABC70
#define __imp__D3DDevice_DrawIndexedVertices               __imp__sub_82BAC140
#define __imp__D3DVertexBuffer_Unlock                      __imp__sub_82BA75B0
#define __imp__D3DIndexBuffer_Unlock                       __imp__sub_82BA1208
#define __imp__D3DDevice_CreateVertexDeclaration           __imp__sub_82BAB638
#define __imp__D3DDevice_CreateVertexShader                __imp__sub_82BABB30
#define __imp__D3DDevice_CreatePixelShader                 __imp__sub_82BAB948
#define __imp__D3DDevice_SetTexture                        __imp__sub_82BA32A0
#define __imp__D3DDevice_SetVertexShader                   __imp__sub_82BAB318
#define __imp__D3DDevice_SetPixelShader                    __imp__sub_82BAB110
#define __imp__D3DDevice_SetVertexDeclaration              __imp__sub_82BAB530
#define __imp__D3DDevice_SetStreamSource                   __imp__sub_82BA5B28
#define __imp__D3DDevice_SetIndices                        __imp__sub_82BA5CD0
