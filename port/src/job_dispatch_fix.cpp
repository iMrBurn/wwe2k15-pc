// wwe2k15 - job hand-off order fix for PCs with few CPU threads
//
// The game hands a job to its helper thread with sub_8261E230 (TU4 address): it sets the job's
// "pending" flag ([obj+100] = 1), wakes the helper (SetEvent on the "go" event [obj+104]) and only
// then clears the job's "done" event ([obj+108], ResetEvent). On the Xbox 360 the helper cannot run
// before the dispatcher's next instructions; on a PC with few cores Windows can run the helper first,
// it finishes and signals "done", and the late reset wipes that signal: the waiter on the done event
// then waits forever (entrances freezing for good on 2-4 thread CPUs).
// Same engine and same function in WWE 2K14 (found there by Jarrod, BSD-3-Clause,
// github.com/Jarrodfeng/wwe2k14-recomp, src/job_dispatch_fix.h); the 2K15 function was found by its shape.
// Fix: clear "done" and set "pending" before waking the helper. Off with wwe2k15_job_reset_first = false.
#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_job_reset_first, true, "WWE2K15",
                    "Clear the job's done event before waking its helper thread (fixes freezes on "
                    "CPUs with few threads)");

REX_EXTERN(__imp__sub_8261E230);
REX_EXTERN(__imp__sub_82262300);  // SetEvent wrapper (r3 = event handle)
REX_EXTERN(__imp__sub_82B89838);  // ResetEvent wrapper (r3 = event handle)

namespace {

constexpr uint32_t kDispatcherGlobal = 0x836F9024;  // lis r11,-31888 ; lwz r31,-28636(r11)

uint32_t Ld32(const uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

void St32(uint8_t* base, uint32_t addr, uint32_t value) {
  value = __builtin_bswap32(value);
  std::memcpy(base + addr, &value, 4);
}

}  // namespace

extern "C" REX_FUNC(sub_8261E230) {
  if (!REXCVAR_GET(wwe2k15_job_reset_first)) {
    __imp__sub_8261E230(ctx, base);
    return;
  }
  const uint32_t obj = Ld32(base, kDispatcherGlobal);
  if (!obj) return;
  const uint32_t go_event = Ld32(base, obj + 104);
  if (!go_event) return;
  const uint32_t done_event = Ld32(base, obj + 108);

  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("job dispatch: done event cleared before waking the helper (obj {:08X})", obj);
  }

  ctx.r3.u64 = done_event;
  __imp__sub_82B89838(ctx, base);  // ResetEvent(done)
  St32(base, obj + 100, 1);        // pending
  ctx.r3.u64 = go_event;
  __imp__sub_82262300(ctx, base);  // SetEvent(go)
}
