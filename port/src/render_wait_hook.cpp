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
#include <rex/logging/macros.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <intrin.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

#include <fmt/format.h>

// On by default since the 2026-10-26 A/B run: same pacing (60 fps in match, 30 in entrances), the
// render thread drops from ~92 % of a core to ~1 % and the whole game from 200 % to 115 %.
REXCVAR_DEFINE_BOOL(wwe2k15_render_wait_yield, true, "WWE2K15",
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

// Issue #33: Create a Championship froze. The game's main thread runs the D3D helper
// sub_82BA8358 (once per texture while the logo list loads) and it writes packets into the same
// D3D command ring as the render thread, with no lock between them; a segment then got closed in
// the middle of the other thread's packet, the command processor dropped its tail (the fences) and
// the game waited forever. Both renderers have the race; native hits it within minutes.
//
// wwe2k15_d3d_ring_lock: a handshake, not a lock held by the render thread. The render thread is
// only between two whole packets at a present and inside its ring and fence waits; a helper call
// waits (at most kHelperWaitMs) until the render thread is parked at one of those points, and the
// render thread stays parked (at most kParkMs) until the helper is done. Past either limit both
// go on as before and it is logged: the worst case is today's behaviour, never a deadlock. Helper
// calls are serialized among themselves.
REXCVAR_DEFINE_BOOL(wwe2k15_d3d_ring_lock, false, "WWE2K15",
                    "Run the game's D3D helper (main thread) only while the render thread is "
                    "between two D3D packets (Create a Championship freeze, issue #33)");

REX_EXTERN(__imp__sub_82BA8358);
REX_EXTERN(__imp__sub_82BB0180);
REX_EXTERN(__imp__sub_82BAFB78);

namespace {
constexpr int kHelperWaitMs = 50;
constexpr int kParkMs = 100;
std::timed_mutex g_helper_mutex;               // one helper call at a time
std::atomic<uint32_t> g_render_thread{0};      // the thread that presents
std::atomic<uint32_t> g_helpers_waiting{0};    // helper calls waiting for a parked render thread
std::atomic<uint32_t> g_helpers_running{0};    // helper calls running under the handshake
std::atomic<bool> g_render_parked{false};
std::atomic<uint32_t> g_helper_calls{0}, g_helper_timeouts{0}, g_park_timeouts{0};
thread_local int t_wait_depth = 0;

uint32_t ThisThread() { return static_cast<uint32_t>(GetCurrentThreadId()); }
bool IsRenderThread() { return g_render_thread.load(std::memory_order_relaxed) == ThisThread(); }
uint64_t NowMs() { return GetTickCount64(); }

// Render thread at a packet boundary: let any running helper finish before going on.
// Leaving: drop the parked flag, then look at the helpers again (both sides use sequentially
// consistent operations: either the helper sees "not parked" or this sees it running).
void RenderUnparkAfterHelpers() {
  const uint64_t start = NowMs();
  for (;;) {
    while (g_helpers_running.load()) {
      if (NowMs() - start > uint64_t(kParkMs)) {
        const uint32_t n = g_park_timeouts.fetch_add(1, std::memory_order_relaxed);
        if (n < 16 || (n & 255) == 0)
          REXLOG_WARN("D3D ring handshake: render thread went on after {} ms ({} times)",
                      kParkMs, n + 1);
        g_render_parked.store(false);
        return;
      }
      SwitchToThread();
    }
    g_render_parked.store(false);
    if (!g_helpers_running.load())
      return;
    g_render_parked.store(true);
  }
}

// Render thread inside one of its waits: parked for the whole wait.
struct RenderWaitScope {
  bool active = false;
  RenderWaitScope() {
    if (REXCVAR_GET(wwe2k15_d3d_ring_lock) && IsRenderThread() && t_wait_depth++ == 0) {
      active = true;
      g_render_parked.store(true);
    }
  }
  ~RenderWaitScope() {
    if (active && --t_wait_depth == 0)
      RenderUnparkAfterHelpers();
  }
};
}  // namespace

// framerate_hook.cpp, once per guest present on the render thread: identify it and, when a helper
// is waiting, park here (the frame's packets are complete) until it has run.
void Wwe2k15RingLockOnPresent() {
  if (!REXCVAR_GET(wwe2k15_d3d_ring_lock))
    return;
  const uint32_t me = ThisThread();
  if (g_render_thread.load(std::memory_order_relaxed) != me) {
    g_render_thread.store(me, std::memory_order_relaxed);
    REXLOG_INFO("D3D ring handshake: render thread {}", me);
  }
  if (!g_helpers_waiting.load(std::memory_order_acquire))
    return;
  g_render_parked.store(true);
  // Give the waiting helper(s) the time to start, then wait for them to finish.
  const uint64_t start = NowMs();
  while (g_helpers_waiting.load(std::memory_order_acquire) &&
         !g_helpers_running.load(std::memory_order_acquire) && NowMs() - start < 5)
    SwitchToThread();
  RenderUnparkAfterHelpers();
}

extern "C" REX_FUNC(sub_82BA8358) {
  if (!REXCVAR_GET(wwe2k15_d3d_ring_lock) || !g_render_thread.load(std::memory_order_relaxed) ||
      IsRenderThread()) {
    __imp__sub_82BA8358(ctx, base);
    return;
  }
  const uint32_t calls = g_helper_calls.fetch_add(1, std::memory_order_relaxed) + 1;
  std::unique_lock serial(g_helper_mutex, std::defer_lock);
  bool ok = serial.try_lock_for(std::chrono::milliseconds(kHelperWaitMs));
  if (ok) {
    g_helpers_waiting.fetch_add(1);
    const uint64_t start = NowMs();
    // Counted as running before the parked check (see RenderUnparkAfterHelpers).
    g_helpers_running.fetch_add(1);
    while (!g_render_parked.load()) {
      g_helpers_running.fetch_sub(1);
      if (NowMs() - start > uint64_t(kHelperWaitMs)) {
        ok = false;
        break;
      }
      SwitchToThread();
      g_helpers_running.fetch_add(1);
    }
    g_helpers_waiting.fetch_sub(1);
  }
  if (!ok) {
    const uint32_t n = g_helper_timeouts.fetch_add(1, std::memory_order_relaxed);
    if (n < 16 || (n & 255) == 0)
      REXLOG_WARN("D3D ring handshake: helper went on alone after {} ms ({} of {} calls)",
                  kHelperWaitMs, n + 1, calls);
    __imp__sub_82BA8358(ctx, base);
    return;
  }
  __imp__sub_82BA8358(ctx, base);
  g_helpers_running.fetch_sub(1);
  if ((calls & 1023) == 0)
    REXLOG_INFO("D3D ring handshake: {} helper calls, {} alone, {} render park timeouts", calls,
                g_helper_timeouts.load(std::memory_order_relaxed),
                g_park_timeouts.load(std::memory_order_relaxed));
}

extern "C" REX_FUNC(sub_82BB0180) {
  RenderWaitScope scope;
  __imp__sub_82BB0180(ctx, base);
}

extern "C" REX_FUNC(sub_82BAFB78) {
  RenderWaitScope scope;
  __imp__sub_82BAFB78(ctx, base);
}
