// wwe2k15 - main menu: SHOP (dead on the PC) becomes EXIT
//
// text_hook.cpp relabels the SHOP string (label string id 59170, descriptor text id 35178) EXIT when wwe2k15_quit_entry is on. The main menu's
// list objects answer A with ConfirmCheckAndPopup(menu r3, group r4, item r5, message override r6)
// = sub_827A3910 (TU4): it fetches the item's descriptor with GetMenuItemDesc = sub_827A0380(menu, group, item)
// (first word 0xFFFFFFFF = invalid, text id at +24) and, for some entries, pushes a dialog request to the game's
// dialog manager. Found with the method of the 2K16/2K17 ports (same family: CheckMenuItemVisible = sub_827A3718).
// With wwe2k15_quit_entry on, choosing the item whose descriptor text id is 35178 asks the app for a Yes/No confirmation
// (an ImGui box, see wwe2k15_app.h) and returns 1 ("handled") instead of running the game's SHOP code; every
// other item goes to the original. wwe2k15_quit_entry_diag logs the calls (menu, group, item, text id).
#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

#include "quit_entry.h"

REXCVAR_DECLARE(bool, wwe2k15_quit_entry);
REXCVAR_DEFINE_BOOL(wwe2k15_quit_entry_diag, false, "WWE2K15",
                    "Debug: log the main menu's A presses (see quit_entry.cpp)");

REX_EXTERN(__imp__sub_827A3910);
REX_EXTERN(__imp__sub_827A0380);

namespace {
constexpr uint32_t kShopTextId = 35178;  // the menu descriptor's id of SHOP (the label string is 59170)
uint32_t Ld32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
}  // namespace

extern "C" REX_FUNC(sub_827A3910) {
  const bool quit = REXCVAR_GET(wwe2k15_quit_entry);
  const bool diag = REXCVAR_GET(wwe2k15_quit_entry_diag);
  if (!quit && !diag) {
    __imp__sub_827A3910(ctx, base);
    return;
  }
  const uint32_t menu = ctx.r3.u32, group = ctx.r4.u32, item = ctx.r5.u32;
  PPCContext c = ctx;
  c.lr = 0;
  __imp__sub_827A0380(c, base);
  const uint32_t desc = c.r3.u32;
  const uint32_t first = desc ? Ld32(base, desc) : 0xFFFFFFFFu;
  const uint32_t text_id = (desc && first != 0xFFFFFFFFu) ? Ld32(base, desc + 24) : 0;
  if (diag) {
    static std::atomic<int> lines{0};
    if (lines.fetch_add(1) < 40)
      REXLOG_INFO("quit entry: ConfirmCheckAndPopup menu {:08X} group {} item {} desc {:08X} text id {} lr {:08X}",
                  menu, group, item, desc, text_id, uint32_t(ctx.lr));
  }
  if (quit && text_id == kShopTextId && Wwe2k15QuitConfirmRequest()) {
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) REXLOG_INFO("quit entry: EXIT chosen on the main menu, asking for confirmation");
    Wwe2k15QuitConfirmRequest()();
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_827A3910(ctx, base);
}
