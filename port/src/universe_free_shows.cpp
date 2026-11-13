// wwe2k15 - EXPERIMENTAL: free Universe shows (wwe2k15_universe_free_shows, default false)
//
// SHOW EDIT's ACCEPT (sub_828484B0, TU4) runs the validator sub_82848228(editor) and refuses the show
// when it returns 0, storing a reason at editor+4892: 4 = a Major show needs a World title, 6/8 = "No
// matches can currently be held for this show" (a configured match slot cannot be filled from the
// roster), 12 = a Minor show has no usable Major host. There is no fixed roster minimum.
// With the cvar on, the validator answers "valid" without running, so a show can be saved with no
// World title, a tiny or empty roster, or no host. Whether the day loop (sub_82A63708) copes with such
// a show is what this experiment tests; keep it off for normal play.
#include "generated/default/wwe2k15_init.h"

#include <atomic>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_universe_free_shows, false, "WWE2K15",
                    "EXPERIMENTAL: Universe SHOW EDIT accepts any show (no World title, tiny/empty roster, "
                    "Minor show without host). May break the Universe day simulation; use a copy of your save");

REXCVAR_DEFINE_BOOL(wwe2k15_universe_free_shows_play, false, "WWE2K15",
                    "EXPERIMENTAL (with wwe2k15_universe_free_shows): the Universe day loop also plays shows that the game "
                    "would skip (Major without World title, Minor without host). May break the day simulation");

REX_EXTERN(__imp__sub_82848228);
REX_EXTERN(__imp__sub_82A73130);

extern "C" REX_FUNC(sub_82848228) {
  if (REXCVAR_GET(wwe2k15_universe_free_shows)) {
    static std::atomic<int> n{0};
    if (n.fetch_add(1) < 20) REXLOG_INFO("[universe] show validator bypassed (free shows)");
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_82848228(ctx, base);
}

// sub_82A73130(show) is the day loop's own validity test (TU4): 0 = playable, 1 = invalid record, 2 = Major whose World
// title is not a live championship, 3 = Major failing sub_82A729D8 or Minor without host. A nonzero answer makes
// sub_82A63708 skip the show (index 255). With wwe2k15_universe_free_shows_play the codes 2 and 3 are turned into 0;
// code 1 (record not valid) is kept, since such a record has no data to play.
extern "C" REX_FUNC(sub_82A73130) {
  __imp__sub_82A73130(ctx, base);
  if (REXCVAR_GET(wwe2k15_universe_free_shows_play) && (ctx.r3.u32 == 2 || ctx.r3.u32 == 3)) {
    static std::atomic<int> n{0};
    if (n.fetch_add(1) < 20) REXLOG_INFO("[universe] day-loop show test {} -> 0 (free shows play)", ctx.r3.u32);
    ctx.r3.u64 = 0;
  }
}
