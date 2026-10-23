// wwe2k15 - render thread ring-wait hook
//
// sub_82BB0180 (base: sub_82BA8208) is the title's D3D command-ring wait: it calls sub_82BAC6B0
// (base: sub_82BA4738) in a loop
// until the GPU's ring read pointer passes a target. On the console that poll burns
// cycles with `db16cyc` delay instructions; here those are no-ops, so the render thread
// spins a whole core at ~90 % while it waits for the host command processor. This hook
// keeps the original function (and its timeout / hang handling) and only adds a short
// spin followed by short sleeps while the wait continues.

#include "generated/default/wwe2k15_init.h"

#include <rex/cvar.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <intrin.h>

#include <atomic>

// Disabled by default until an A/B run shows it does not change pacing (PORTING_PLAN.md rules).
REXCVAR_DEFINE_BOOL(wwe2k15_render_wait_yield, false, "WWE2K15",
                    "Let the render thread sleep briefly while waiting for the GPU ring");

REX_EXTERN(__imp__sub_82BAC6B0);

namespace {

constexpr double kSpinUs = 150.0;    // keep the latency of short waits unchanged
constexpr double kSleepUs = 250.0;   // then wait in small slices
constexpr double kNewWaitGapUs = 2000.0;

struct WaitState {
  uint64_t qpc_freq = 0;
  uint64_t last_call = 0;
  uint64_t wait_start = 0;
  void* timer = nullptr;
};

thread_local WaitState state;

void SleepSlice(WaitState& s) {
#if defined(_WIN32)
  if (!s.timer) {
    s.timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_ALL_ACCESS);
  }
  if (s.timer) {
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(kSleepUs * 10.0);  // 100 ns units, relative
    if (SetWaitableTimer(s.timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(s.timer, 5);
      return;
    }
  }
  SwitchToThread();
#endif
}

}  // namespace

// Microseconds the render thread spent waiting for the GPU ring (diagnostics, read by
// framerate_hook.cpp).
std::atomic<uint64_t> g_wwe2k15_gpu_wait_us{0};

namespace {
thread_local bool t_in_wait = false;
thread_local uint64_t t_wait_begin = 0;
uint64_t QpcUs() {
  LARGE_INTEGER f, c;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<uint64_t>(c.QuadPart * 1000000.0 / f.QuadPart);
}
}  // namespace

extern "C" REX_FUNC(sub_82BAC6B0) {
  __imp__sub_82BAC6B0(ctx, base);
  if (ctx.r3.u32 == 1 && !t_in_wait) {
    t_in_wait = true;
    t_wait_begin = QpcUs();
  } else if (ctx.r3.u32 != 1 && t_in_wait) {
    t_in_wait = false;
    g_wwe2k15_gpu_wait_us.fetch_add(QpcUs() - t_wait_begin, std::memory_order_relaxed);
  }
  // r3 == 1: the poll says the GPU has not caught up yet, so the caller will call again.
  if (ctx.r3.u32 != 1 || !REXCVAR_GET(wwe2k15_render_wait_yield)) {
    return;
  }
#if defined(_WIN32)
  WaitState& s = state;
  if (!s.qpc_freq) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    s.qpc_freq = static_cast<uint64_t>(f.QuadPart);
  }
  LARGE_INTEGER now_li;
  QueryPerformanceCounter(&now_li);
  const uint64_t now = static_cast<uint64_t>(now_li.QuadPart);
  const double us_per_tick = 1e6 / static_cast<double>(s.qpc_freq);
  if ((now - s.last_call) * us_per_tick > kNewWaitGapUs) {
    s.wait_start = now;  // a new wait began; the previous one ended
  }
  s.last_call = now;
  if ((now - s.wait_start) * us_per_tick > kSpinUs) {
    SleepSlice(s);
  } else {
    _mm_pause();
  }
#endif
}
