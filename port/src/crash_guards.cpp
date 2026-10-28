// wwe2k15 - guards for recompiled game functions that read a pointer the game left NULL
//
// Each guard hooks one function (TU4 address; the base-game address is in the comment), checks the
// pointers the function is about to follow and, when one is NULL, skips the call (the function
// returns nothing the callers use) and logs once. Without it the read of guest address 0x1C..., i.e.
// host guest-base + small offset, is an access violation that ends the game.
#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_crash_guards, true, "WWE2K15",
                    "Skip game calls that would dereference a NULL pointer (crash guards)");

REX_EXTERN(__imp__sub_8253D490);

namespace {

uint32_t Ld32(const uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

}  // namespace

// sub_8253D490(object r3): checks the object's 'owner' and, when allowed, calls sub_8253CF30. It
// follows [r3+28], [r3+32], [r3+36] and [[r3+2712]+28] without testing them. Issue #11: a crash
// right after a Money in the Bank ladder match in Universe mode, read of guest 0x1C (a NULL
// [r3+2712] or a NULL [[r3+2712]+28]) in wwe2k15_native.exe (the same code runs in wwe2k15.exe).
extern "C" REX_FUNC(sub_8253D490) {
  const uint32_t obj = ctx.r3.u32;
  if (REXCVAR_GET(wwe2k15_crash_guards) && obj >= 0x10000) {
    const uint32_t owner = Ld32(base, obj + 2712);
    const bool ok = Ld32(base, obj + 28) && Ld32(base, obj + 32) && Ld32(base, obj + 36) && owner &&
                    Ld32(base, owner + 28);
    if (!ok) {
      static std::atomic<uint32_t> skipped{0};
      if (skipped.fetch_add(1) < 5)
        REXLOG_WARN("crash guard: sub_8253D490 skipped, object {:08X} has a NULL field "
                    "([+28] {:08X} [+32] {:08X} [+36] {:08X} [+2712] {:08X}) lr {:08X}", obj,
                    Ld32(base, obj + 28), Ld32(base, obj + 32), Ld32(base, obj + 36), owner,
                    uint32_t(ctx.lr));
      return;
    }
  }
  __imp__sub_8253D490(ctx, base);
}
