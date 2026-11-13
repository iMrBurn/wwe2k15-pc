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

REX_EXTERN(__imp__sub_82848228);

extern "C" REX_FUNC(sub_82848228) {
  if (REXCVAR_GET(wwe2k15_universe_free_shows)) {
    static std::atomic<int> n{0};
    if (n.fetch_add(1) < 20) REXLOG_INFO("[universe] show validator bypassed (free shows)");
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_82848228(ctx, base);
}
