// wwe2k15 - frame-rate control
//
// sub_82AB14C8(fps) is the title's SetFrameRate (TU4 address): it accepts 25/30/50/60 and
// fills the timing struct at 0x838DC7A8 (+8 = fps, +40 = ms per frame, float steps at
// +28/+32/+60/+64), the same design as WWE '12's sub_82827918. Game logic and the present
// interval both follow it, so forcing 60 here (cvar wwe2k15_60fps) keeps animation speed
// correct, unlike only presenting more often.

#include "generated/default/wwe2k15_init.h"

#include <algorithm>
#include <atomic>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_show_fps, false, "WWE2K15", "Show an FPS counter in the game");
REXCVAR_DEFINE_BOOL(wwe2k15_mods, true, "WWE2K15", "Load replacement files from the mods folder");

REXCVAR_DEFINE_BOOL(wwe2k15_60fps, false, "WWE2K15",
                    "Keep the game at 60 FPS (50 on PAL) when it asks for 30 (25)");

REX_EXTERN(__imp__sub_82AB14C8);
REX_EXTERN(__imp__sub_82BADF20);

extern std::atomic<uint64_t> g_wwe2k15_gpu_wait_us;

// Guest presents per second (x10) and the worst frame time (us) over the last second, for the
// in-game FPS counter (wwe2k15_app.h).
std::atomic<uint32_t> g_wwe2k15_fps_x10{0};
std::atomic<uint32_t> g_wwe2k15_worst_frame_us{0};
std::atomic<uint64_t> g_wwe2k15_present_count{0};  // heartbeat for the freeze watchdog

namespace {
std::atomic<uint32_t> g_calls{0};
}

extern "C" REX_FUNC(sub_82AB14C8) {
  const uint32_t requested = ctx.r3.u32;
  uint32_t applied = requested;
  if (REXCVAR_GET(wwe2k15_60fps)) {
    if (requested == 30) applied = 60;
    else if (requested == 25) applied = 50;
  }
  const uint32_t n = g_calls.fetch_add(1);
  if (n < 32 || n % 100 == 0) {
    REXLOG_INFO("SetFrameRate({}) -> {} (call {}, lr {:08X})", requested, applied, n,
                uint32_t(ctx.lr));
  }
  ctx.r3.u64 = applied;
  __imp__sub_82AB14C8(ctx, base);
}

// D3D present (sub_82BADF20, base sub_82BA5FA8). Every 5 s, log the guest frame pacing:
// frames, mean/max time between presents, share of it spent waiting for the GPU ring, and
// the fps the title's timing struct asks for.
namespace {
uint64_t NowUs() {
  LARGE_INTEGER f, c;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<uint64_t>(c.QuadPart * 1000000.0 / f.QuadPart);
}
struct PaceStats {
  uint64_t window_start = 0, last = 0, max_gap = 0, wait_at_start = 0;
  uint32_t frames = 0;
} g_pace;
struct Counter {
  uint64_t start = 0, worst = 0;
  uint32_t frames = 0;
} g_counter;
}  // namespace

// wwe2k15_native.exe: re:Blue overrides D3DDevice_Swap (sub_82BADF20) itself, so this pacing hook
// is left out there (the cvars and counters above stay, the app and launcher read them).
#if !defined(SVR_NATIVE_RENDERER)
extern "C" REX_FUNC(sub_82BADF20) {
  g_wwe2k15_present_count.fetch_add(1, std::memory_order_relaxed);
  const uint64_t now = NowUs();
  Counter& c = g_counter;
  if (!c.start) {
    c.start = now;
  } else {
    ++c.frames;
    if (g_pace.last) c.worst = std::max(c.worst, now - g_pace.last);
    if (now - c.start >= 1000000) {
      g_wwe2k15_fps_x10.store(uint32_t(c.frames * 1e7 / double(now - c.start)));
      g_wwe2k15_worst_frame_us.store(uint32_t(c.worst));
      c = Counter{};
      c.start = now;
    }
  }
  PaceStats& p = g_pace;
  if (!p.window_start) {
    p.window_start = p.last = now;
    p.wait_at_start = g_wwe2k15_gpu_wait_us.load();
  } else {
    p.max_gap = std::max(p.max_gap, now - p.last);
    p.last = now;
    ++p.frames;
    const uint64_t span = now - p.window_start;
    if (span >= 5000000) {
      const uint64_t wait = g_wwe2k15_gpu_wait_us.load() - p.wait_at_start;
      REXLOG_INFO("Guest pacing: {} frames in {:.1f} s ({:.1f} fps), max gap {:.1f} ms, "
                  "GPU-ring wait {:.0f}%, title fps {}",
                  p.frames, span / 1e6, p.frames * 1e6 / span, p.max_gap / 1000.0,
                  100.0 * wait / span, REX_LOAD_U32(0x838DC7B0));
      p = PaceStats{};
      p.window_start = p.last = now;
      p.wait_at_start = g_wwe2k15_gpu_wait_us.load();
    }
  }
  __imp__sub_82BADF20(ctx, base);
}
#endif  // !SVR_NATIVE_RENDERER
