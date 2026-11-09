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

REXCVAR_DEFINE_BOOL(wwe2k15_60fps, true, "WWE2K15",
                    "Entrances and victory scenes at 60 FPS (50 on PAL) instead of the console's 30 (25); false = original 30");

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


// "Drop to 30 (25) fps" (sub_82AB1668, TU4) and "back to 60 (50)" (sub_82AB1708): the game drops to 30
// for entrances and victory scenes by rewriting its timing struct (0x838DC7A8: +8 fps, +28/+32/+36/+60/+64
// per-frame steps, +40 ms per frame; same design as WWE '12's sub_82827AB8/B58). With wwe2k15_60fps the drop
// is skipped, so game logic and presents both stay at 60 and the scene plays at the right speed.
REX_EXTERN(__imp__sub_82AB1668);
REX_EXTERN(__imp__sub_82AB1708);

REXCVAR_DEFINE_INT32(wwe2k15_60fps_auto_ms, 26, "WWE2K15",
                     "Average frame time (ms) above which a scene at 60 fps is given up (wwe2k15_60fps_auto)")
    .range(10, 60);

REXCVAR_DEFINE_BOOL(wwe2k15_60fps_auto, true, "WWE2K15",
                    "If an entrance/victory scene cannot hold 60 fps (slow GPU or CPU: the scene would run in slow motion), "
                    "go back to the game's 30 fps for it and the rest of the session");

namespace {
std::atomic<bool> g_scene_at60{false};
std::atomic<uint64_t> g_scene_start_us{0};
std::atomic<bool> g_auto_off{false};  // a PC that cannot hold 60 in scenes: they stay at the game's 30
PPCContext g_drop_ctx;                 // registers at the last "drop to 30" call (to run it later)
uint8_t* g_drop_base = nullptr;
uint64_t NowUsEarly() {
  LARGE_INTEGER f, c;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<uint64_t>(c.QuadPart * 1000000.0 / f.QuadPart);
}
}  // namespace

// True while an entrance/victory scene that the game wants at 30 runs at 60 instead.
bool Wwe2k15SceneAt60() { return g_scene_at60.load(std::memory_order_relaxed); }

extern "C" REX_FUNC(sub_82AB1668) {
  const uint64_t mode = REX_LOAD_U32(0x838DC7B0);
  if (REXCVAR_GET(wwe2k15_60fps) && !g_auto_off.load(std::memory_order_relaxed)) {
    g_drop_ctx = ctx;
    g_drop_base = base;
    g_scene_at60.store(true, std::memory_order_relaxed);
    g_scene_start_us.store(NowUsEarly(), std::memory_order_relaxed);
    REXLOG_INFO("[fps] game drops to 30 (mode {}): kept at 60", mode);
    return;
  }
  g_scene_start_us.store(NowUsEarly(), std::memory_order_relaxed);
  REXLOG_INFO("[fps] game drops to 30 (mode {})", mode);
  __imp__sub_82AB1668(ctx, base);
}

extern "C" REX_FUNC(sub_82AB1708) {
  const uint64_t start = g_scene_start_us.exchange(0, std::memory_order_relaxed);
  if (start) REXLOG_INFO("[fps] back to 60 after {:.2f} s", (NowUsEarly() - start) / 1e6);
  g_scene_at60.store(false, std::memory_order_relaxed);
  __imp__sub_82AB1708(ctx, base);
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
// Called once per present. While a scene runs at 60 instead of the game's 30, if the average frame time stays
// above 26 ms for two windows of 90 frames the PC cannot hold it (the game's logic follows the frame rate, so
// the scene would play in slow motion): run the game's own "drop to 30" now and keep it for the session.
void SceneWatch(uint64_t now) {
  static uint64_t last = 0, sum = 0;
  static uint32_t frames = 0, slow = 0;
  if (!g_scene_at60.load(std::memory_order_relaxed)) {
    last = 0, sum = 0, frames = 0, slow = 0;
    return;
  }
  if (last) {
    sum += now - last;
    ++frames;
  }
  last = now;
  if (frames < 90) return;
  const double avg_ms = sum / 1000.0 / frames;
  sum = 0, frames = 0;
  slow = avg_ms > REXCVAR_GET(wwe2k15_60fps_auto_ms) ? slow + 1 : 0;
  if (slow < 2 || !REXCVAR_GET(wwe2k15_60fps_auto) || !g_drop_base) return;
  g_scene_at60.store(false, std::memory_order_relaxed);
  g_auto_off.store(true, std::memory_order_relaxed);
  PPCContext c = g_drop_ctx;
  __imp__sub_82AB1668(c, g_drop_base);
  REXLOG_INFO("[fps] scene cannot hold 60 fps ({:.1f} ms per frame): the game's 30 fps is used for scenes from now on",
              avg_ms);
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

// Called once per guest present: by the Swap hook below, or in wwe2k15_native.exe (where re:Blue
// overrides D3DDevice_Swap itself) from the renderer's per-frame tick (Wwe12DisplayOnFrame). `base`
// is null there: the title fps read from guest memory is then left out of the log line.
void Wwe2k15RingLockOnPresent();  // render_wait_hook.cpp (issue #33)

void Wwe2k15OnGuestPresent(uint8_t* base) {
  Wwe2k15RingLockOnPresent();
  g_wwe2k15_present_count.fetch_add(1, std::memory_order_relaxed);
  const uint64_t now = NowUs();
  SceneWatch(now);
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
                  100.0 * wait / span, base ? REX_LOAD_U32(0x838DC7B0) : 0u);
      p = PaceStats{};
      p.window_start = p.last = now;
      p.wait_at_start = g_wwe2k15_gpu_wait_us.load();
    }
  }
}

#if !defined(SVR_NATIVE_RENDERER)
extern "C" REX_FUNC(sub_82BADF20) {
  Wwe2k15OnGuestPresent(base);
  __imp__sub_82BADF20(ctx, base);
}
#endif  // !SVR_NATIVE_RENDERER
