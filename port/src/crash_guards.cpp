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
REX_EXTERN(__imp__sub_82AAFCE8);

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

// sub_82AAFCE8(this r3, cmd slot r4): runs one command of the game's display/script command list
// (type = [[r4]], jump table of 105 types; the list is walked by sub_82AB0500 on the render
// thread, ends on type 104 or 105). Type 43 is "call a method": it loads the 8-byte field at
// cmd+16 (object in the low word, cmd+20) and does `obj->vtable[7](obj, [cmd+24], 0)` without
// testing anything. Issues #39 (read 0x52) and #44 (read 0x1C), Showcase / story cutscenes: the
// object had been freed / cleared (its vtable word is 0), so the read of guest 0x1C faults.
// Guard 1: type 43 with a missing object, vtable or method is skipped (logged).
// Guard 2 (issue #45, a render thread spinning at 100 % CPU in this loop for 20 s+): when the
// same command slot is run many times in a row the list is not advancing (a corrupted size
// field), so the command is turned into a type 104 terminator, which ends that buffer.
extern "C" REX_FUNC(sub_82AAFCE8) {
  if (REXCVAR_GET(wwe2k15_crash_guards) && ctx.r4.u32 >= 0x10000) {
    const uint32_t cmd = Ld32(base, ctx.r4.u32);
    if (cmd >= 0x10000) {
      const uint32_t type = Ld32(base, cmd);
      if (type == 43) {
        const uint32_t obj = Ld32(base, cmd + 20);
        const uint32_t vt = obj >= 0x10000 ? Ld32(base, obj) : 0;
        const uint32_t fn = vt >= 0x10000 ? Ld32(base, vt + 28) : 0;
        if (fn < 0x82000000 || fn >= 0x83000000) {
          static std::atomic<uint32_t> skipped{0};
          if (skipped.fetch_add(1) < 8)
            REXLOG_WARN("crash guard: command-list call skipped (sub_82AAFCE8 type 43), object {:08X} "
                        "vtable {:08X} method {:08X} cmd {:08X} lr {:08X}", obj, vt, fn, cmd,
                        uint32_t(ctx.lr));
          return;
        }
      }
      thread_local uint32_t last_cmd = 0;
      thread_local uint32_t repeats = 0;
      if (cmd == last_cmd) {
        if (++repeats == 200000) {
          REXLOG_ERROR("crash guard: command list stuck on {:08X} (type {}), ending the buffer", cmd,
                       type);
          const uint32_t v = __builtin_bswap32(104);
          std::memcpy(const_cast<uint8_t*>(base) + cmd, &v, 4);
          repeats = 0;
          return;
        }
      } else {
        last_cmd = cmd;
        repeats = 0;
      }
    }
  }
  __imp__sub_82AAFCE8(ctx, base);
}
