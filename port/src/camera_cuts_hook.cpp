// wwe2k15 - camera cuts on finishers and signatures (issues #19, #32)
//
// The pause menu option VISUAL PRESENTATION > CAMERA CUTS is the byte at settings+15204
// (sub_827409F0 returns the settings object); it is read normally. What blocks the cuts on finishers
// and signatures is the tail of sub_824BB130(actor), the "may the camera cut now?" predicate
// (TU4 addresses): after its other tests it compares the actor's current move id
// (*(actor+28) -> u16) with sub_824DB0F8(actor, k) for k = 112-115, 91-94, 98-101, 105-108 (sixteen
// calls, return addresses 0x824BB570 .. 0x824BB6E4) and returns 0 (no cut) on a match. sub_824DB0F8
// reads the u16 at moveset + (k + 222) * 2: those are the signature / finisher move slots.
// With wwe2k15_camera_cuts_on_finishers the sixteen calls made from that tail answer 0xFFFF, which is
// never a move id, so the rest of the predicate (and the option itself) work as before. Every other
// caller of sub_824DB0F8 is passed through untouched.
//
// wwe2k15_camera_cuts_diag (debug, off by default) logs, once per (move id, result, reason), what the
// predicate answered: "cut" (1), "refused in the slot test" (0 with the slot that matched) or "refused
// before the slot test" (0 from an earlier test), with the sixteen slot values of the actor.
#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_camera_cuts_on_finishers, false, "WWE2K15",
                    "Let the Camera Cuts option also cut on signature and finisher moves (the TU4 game never does)");
REXCVAR_DEFINE_BOOL(wwe2k15_camera_cuts_on_holds, false, "WWE2K15",
                    "Let the Camera Cuts option also cut while two wrestlers are locked in a grapple/hold (the TU4 game never does)");
REXCVAR_DEFINE_BOOL(wwe2k15_camera_cuts_diag, false, "WWE2K15",
                    "Debug: log the camera cut predicate's answers per move id (see camera_cuts_hook.cpp)");

REX_EXTERN(__imp__sub_824DB0F8);
REX_EXTERN(__imp__sub_824BB130);
REX_EXTERN(__imp__sub_824AE850);
REX_EXTERN(__imp__sub_824CC060);
REX_EXTERN(__imp__sub_827409F0);
REX_EXTERN(__imp__sub_824C28C8);
REX_EXTERN(__imp__sub_824C3DE0);

namespace {
constexpr uint32_t kTailFirstReturn = 0x824BB570;
constexpr uint32_t kTailLastReturn = 0x824BB6E4;
constexpr int kSlots[16] = {112, 113, 114, 115, 91, 92, 93, 94, 98, 99, 100, 101, 105, 106, 107, 108};

std::atomic<bool> g_logged{false};
std::atomic<uint32_t> g_diag_lines{0};
// Per move id: bit0 = answered 1, bit1 = refused before the slot test, bit2 = refused in the slot test.
std::atomic<uint8_t> g_seen[65536];
// The slot that matched during the current predicate call on this thread (0 = none).
thread_local uint32_t t_matched_slot = 0;
thread_local uint32_t t_matched_value = 0;
// Opponent move id when the opponent loop of the predicate ended in a refusal (0 = it did not).
thread_local uint32_t t_loop_refusal = 0;
constexpr uint32_t kLoopLastCall = 0x824BB534;  // return address of sub_824CC060 in the predicate

inline uint32_t Ld32(const uint8_t* base, uint32_t a) {
  return __builtin_bswap32(*reinterpret_cast<const volatile uint32_t*>(base + a));
}
inline uint32_t Ld16(const uint8_t* base, uint32_t a) {
  return __builtin_bswap16(*reinterpret_cast<const volatile uint16_t*>(base + a));
}
}  // namespace

extern "C" REX_FUNC(sub_824DB0F8) {
  const uint32_t lr = uint32_t(ctx.lr);
  if (lr < kTailFirstReturn || lr >= kTailLastReturn) {
    __imp__sub_824DB0F8(ctx, base);
    return;
  }
  const bool enabled = REXCVAR_GET(wwe2k15_camera_cuts_on_finishers);
  if (!enabled && !REXCVAR_GET(wwe2k15_camera_cuts_diag)) {
    __imp__sub_824DB0F8(ctx, base);  // default: plain pass-through, nothing logged
    return;
  }
  const uint32_t actor = ctx.r3.u32;
  const uint32_t slot = ctx.r4.u32;
  __imp__sub_824DB0F8(ctx, base);
  if (!g_logged.exchange(true) && enabled) {
    REXLOG_INFO("camera cuts on finishers enabled (cut test hooked, first call slot {}, lr {:08X})", slot, lr);
  }
  const uint32_t info = actor ? Ld32(base, actor + 28) : 0;
  const uint32_t move = info ? Ld16(base, info) : 0xFFFFFFFFu;
  if (move == (ctx.r3.u32 & 0xFFFF)) {
    t_matched_slot = slot;
    t_matched_value = move;
    if (enabled) ctx.r3.u64 = 0xFFFF;
  }
}

// sub_824CC060(actor) = classify the actor's current move id (tail call to sub_824C51B0). The predicate calls it last in
// its loop over the other wrestlers; a 0 there ends the predicate with "no cut".
extern "C" REX_FUNC(sub_824CC060) {
  const uint32_t lr = uint32_t(ctx.lr);
  const uint32_t other = ctx.r3.u32;
  __imp__sub_824CC060(ctx, base);
  if (lr == kLoopLastCall && ctx.r3.u32 == 0 && REXCVAR_GET(wwe2k15_camera_cuts_diag)) {
    const uint32_t info = other ? Ld32(base, other + 28) : 0;
    t_loop_refusal = info ? (Ld16(base, info) | 0x10000u) : 0x1FFFFu;
  }
}

// sub_824AE850(x) = "x is in a hold" (virtual 128 of the wrestler class). The predicate refuses a cut when the actor AND its
// partner (actor+836) are both in a hold: actor call at lr 0x824BB1EC, partner call at lr 0x824BB214. With
// wwe2k15_camera_cuts_on_holds the partner answers "not in a hold" there, so that test can never refuse; every other call
// of sub_824AE850 is passed through.
extern "C" REX_FUNC(sub_824AE850) {
  if (uint32_t(ctx.lr) == 0x824BB214 && REXCVAR_GET(wwe2k15_camera_cuts_on_holds)) {
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) REXLOG_INFO("camera cuts on holds enabled (partner hold test skipped)");
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_824AE850(ctx, base);
}

// Diagnostic wrapper around the predicate itself (pass-through unless wwe2k15_camera_cuts_diag).
extern "C" REX_FUNC(sub_824BB130) {
  if (!REXCVAR_GET(wwe2k15_camera_cuts_diag)) {
    __imp__sub_824BB130(ctx, base);
    return;
  }
  const uint32_t actor = ctx.r3.u32;
  t_matched_slot = 0;
  t_loop_refusal = 0;
  __imp__sub_824BB130(ctx, base);
  const uint32_t result = ctx.r3.u32;
  const uint32_t info = actor ? Ld32(base, actor + 28) : 0;
  if (!info) return;
  const uint32_t move = Ld16(base, info);
  const bool slot_match = t_matched_slot != 0;
  // Reason codes: 0 cut, 1 cut (slot match let through), 2 refused in the slot test, 3 refused at the end of the
  // opponent loop, 4 refused by the game-mode global, 5 refused by settings byte +14, 6 refused elsewhere.
  uint32_t code;
  char detail[240] = "";
  if (result) {
    code = slot_match ? 1 : 0;
  } else if (slot_match) {
    code = 2;
  } else if (t_loop_refusal) {
    code = 3;
    snprintf(detail, sizeof detail, " (opponent move %u)", t_loop_refusal & 0xFFFF);
  } else {
    const uint32_t mode = Ld32(base, 0x83680000u + 16240);
    PPCContext c = ctx;
    c.lr = 0;
    __imp__sub_827409F0(c, base);
    const uint32_t settings = c.r3.u32;
    if (mode == 2 || mode == 4) code = 4;
    else if (settings && *(base + settings + 14)) code = 5;
    else code = 6;
    if (code == 6 && settings) {
      // State the early tests read (see sub_824BB130): settings bytes, the actor's own flags, the number of
      // wrestlers in play (r29) and of "active" ones (r27), as the predicate counts them.
      uint32_t r27 = 0, r29 = 0;
      for (int i = 0; i < 7; ++i) {
        const uint32_t a = Ld32(base, 0x83683F00u + 4u * i);
        if (!a) continue;
        const uint32_t st = Ld32(base, a + 3752);
        if (Ld32(base, a + 224) && !*(base + a + 70) && st != 1 && st != 6) ++r27;
        if (st != 4 && st != 6) ++r29;
      }
      PPCContext c1 = ctx;
      c1.lr = 0;
      c1.r3.u64 = move;
      __imp__sub_824C28C8(c1, base);
      PPCContext c2 = ctx;
      c2.lr = 0;
      c2.r3.u64 = actor;
      __imp__sub_824C3DE0(c2, base);
      // Test E of the predicate: actor->vfunc128() && partner(+836) && partner->vfunc128() -> no cut.
      const uint32_t partner = Ld32(base, actor + 836);
      const uint32_t vf = Ld32(base, Ld32(base, actor) + 128);
      const uint32_t pvf = partner ? Ld32(base, Ld32(base, partner) + 128) : 0;
      char ex[64];
      snprintf(ex, sizeof ex, " partner %08X vf %08X pvf %08X", partner, vf, pvf);
      snprintf(detail, sizeof detail, " [s+41 %u s+6 %u s+33 %u s+60 %u a+70 %u c28c8 %u c3de0 %u st %u n %u/%u]",
               *(base + settings + 41), *(base + settings + 6), *(base + settings + 33), *(base + settings + 60),
               *(base + actor + 70), c1.r3.u32, c2.r3.u32, Ld32(base, actor + 3752), r29, r27);
      strncat(detail, ex, sizeof detail - strlen(detail) - 1);
    }
  }
  const uint8_t bit = uint8_t(1u << code);
  if (g_seen[move].fetch_or(bit) & bit) return;
  if (g_diag_lines.fetch_add(1) >= 600) return;
  static const char* const kWhat[] = {"cut", "cut (slot match let through)", "REFUSED in the slot test",
                                      "refused at the end of the opponent loop", "refused: game-mode global 2/4",
                                      "refused: settings byte +14", "refused: other early test"};
  // The sixteen slot values of this actor (call the original with a scratch context: no hook, no side effect).
  uint32_t vals[16];
  for (int i = 0; i < 16; ++i) {
    PPCContext c = ctx;
    c.lr = 0;
    c.r3.u64 = actor;
    c.r4.u64 = uint64_t(kSlots[i]);
    __imp__sub_824DB0F8(c, base);
    vals[i] = c.r3.u32 & 0xFFFF;
  }
  REXLOG_INFO(
      "camera cut predicate: move {} -> {} {}{} slot {} | slots 112-115 {} {} {} {} | 91-94 {} {} {} {} | "
      "98-101 {} {} {} {} | 105-108 {} {} {} {}",
      move, result, kWhat[code], detail, t_matched_slot, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6],
      vals[7], vals[8], vals[9], vals[10], vals[11], vals[12], vals[13], vals[14], vals[15]);
}
