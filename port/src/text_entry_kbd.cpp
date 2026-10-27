// WWE 2K15 PC: type with the physical keyboard in the game's own letter grid (CAW names, Create a
// Story text, titles...), no controller needed.
//
// How it works (found 2026-10-27 with tools/dev/memtrap.cpp, see docs/PORTING_PLAN.md):
//   - 2K15 does not use XamShowKeyboardUI for these screens: it has its own on-screen letter grid,
//     as WWE '12 does. The grid's "editor" object holds the text; sub_828A2CA8(editor, const char* s,
//     1, 0) inserts s at the cursor (the same call the grid's letter cells make, from the button
//     handler sub_828A6478(editor, code)), and BACK is code 14 of that handler (sub_828A4910).
//   - Keys come from the game window (OnKeyChar = the text Windows produces, so AZERTY, Shift,
//     AltGr and key repeat all work). They are queued and replayed through those functions by the
//     game's own thread, inside a hook, exactly as if the grid's buttons were pressed.
//   - While the grid is up, the typing keys are not passed to mnk_mode (the keyboard-as-controller),
//     so Z or Backspace type instead of moving or going back. Enter, Escape and the arrows still
//     reach the game.
// Disable with wwe2k15_kbd = false in wwe2k15.toml. Works in both exes (no renderer code).
//
// Dev probe (wwe2k15_kbd_log = true): logs the editor and every insert / button of the game.

#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

REXCVAR_DEFINE_BOOL(wwe2k15_kbd, true, "WWE2K15",
                    "Type with the physical keyboard in the game's text screens.");
REXCVAR_DEFINE_BOOL(wwe2k15_kbd_log, false, "WWE2K15",
                    "Log the text screen's editor and the game's inserts / buttons (probe).");

REX_EXTERN(__imp__sub_828A2CA8);  // grid insert: (editor, const char* s, 1, 0)
REX_EXTERN(__imp__sub_828A6478);  // grid button handler: (editor, code)
REX_EXTERN(__imp__sub_82333D98);  // text widget draw: (screen, widget), every frame

namespace {

constexpr char kBackspace = '\b';
constexpr uint32_t kButtonBack = 14;           // code of the BACK button in sub_828A6478
constexpr long long kScreenTimeoutMs = 600;    // seen less than this long ago = still open

rex::ui::Window* g_window = nullptr;
std::atomic<uint32_t> g_editor{0};      // guest address of the grid's editor object
std::atomic<long long> g_last_seen{0};  // ms, steady clock

std::mutex g_queue_mutex;
std::deque<char> g_queue;  // typed characters (or kBackspace) waiting for the game thread

[[nodiscard]] long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

[[nodiscard]] bool GridOpen() {
  return REXCVAR_GET(wwe2k15_kbd) && g_editor.load() != 0 &&
         NowMs() - g_last_seen.load() < kScreenTimeoutMs;
}

// The game's font is ASCII: accented letters become their plain letter (é -> e), the rest is dropped.
[[nodiscard]] char ToGameChar(uint32_t cp) {
  if (cp >= 0x20 && cp < 0x7F) return char(cp);
  switch (cp) {
    case 0xE0: case 0xE1: case 0xE2: case 0xE4: return 'a';
    case 0xC0: case 0xC1: case 0xC2: case 0xC4: return 'A';
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
    case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'E';
    case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';
    case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'I';
    case 0xF2: case 0xF3: case 0xF4: case 0xF6: return 'o';
    case 0xD2: case 0xD3: case 0xD4: case 0xD6: return 'O';
    case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';
    case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'U';
    case 0xE7: return 'c';
    case 0xC7: return 'C';
    case 0xF1: return 'n';
    case 0xD1: return 'N';
    default: return 0;
  }
}

// Keys that type while the grid is open (kept away from mnk_mode). Win32 virtual-key codes.
[[nodiscard]] bool IsTypingKey(uint16_t vk) {
  return vk == 0x08 /*Backspace*/ || vk == 0x20 /*Space*/ || (vk >= '0' && vk <= '9') ||
         (vk >= 'A' && vk <= 'Z') || (vk >= 0x60 && vk <= 0x6F) /*numpad*/ ||
         (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2 /*OEM*/;
}

class KbdListener final : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (!GridOpen() || e.is_ctrl_pressed()) return;
    const auto vk = static_cast<uint16_t>(e.virtual_key());
    if (!IsTypingKey(vk)) return;
    if (vk == 0x08) Push(kBackspace);
    e.set_handled(true);
  }

  void OnKeyChar(rex::ui::KeyEvent& e) override {
    if (!GridOpen()) return;
    if (const char ch = ToGameChar(static_cast<uint32_t>(e.virtual_key()))) Push(ch);
    e.set_handled(true);
  }

 private:
  static void Push(char ch) {
    std::lock_guard lock(g_queue_mutex);
    if (g_queue.size() < 256) g_queue.push_back(ch);
  }
};

KbdListener g_listener;

// SDL only sends typed text while text input is on; the SDK's ImGui turns it off when its own
// text boxes close, so it is asked again whenever the grid is seen with it off.
void KeepTextInputOn() {
  rex::ui::Window* window = g_window;
  if (!window || window->IsTextInputActive()) return;
  window->app_context().CallInUIThread([window] { window->SetTextInputActive(true); });
}

uint32_t Ld32(const uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

// Game thread: replay the queued keys through the grid's own functions, as its buttons would.
void ApplyQueued(PPCContext& ctx, uint8_t* base, uint32_t editor) {
  std::deque<char> keys;
  {
    std::lock_guard lock(g_queue_mutex);
    keys.swap(g_queue);
  }
  for (const char ch : keys) {
    PPCContext c = ctx;
    // A scratch area below the caller's frame: the string at +256, the callee's frames below.
    c.r1.u64 = (ctx.r1.u32 - 512) & ~15u;
    c.r3.u64 = editor;
    if (ch == kBackspace) {
      c.r4.u64 = kButtonBack;
      __imp__sub_828A6478(c, base);
    } else {
      const uint32_t str = c.r1.u32 + 256;
      base[str] = uint8_t(ch);
      base[str + 1] = 0;
      c.r4.u64 = str;
      c.r5.u64 = 1;
      c.r6.u64 = 0;
      __imp__sub_828A2CA8(c, base);
    }
  }
  if (!keys.empty() && REXCVAR_GET(wwe2k15_kbd_log)) {
    REXLOG_INFO("[kbd] {} key(s) replayed in editor {:08X}", keys.size(), editor);
  }
}

}  // namespace

// Call once the window exists (Wwe2k15App::OnPostSetup).
void Wwe2k15KbdInit(rex::ui::Window* window) {
  g_window = window;
  if (window) window->AddInputListener(&g_listener, 1000);  // before mnk_mode's listener
}

// Probes (wwe2k15_kbd_log): the game's own button presses and inserts.
extern "C" REX_FUNC(sub_828A6478) {
  const uint32_t editor = ctx.r3.u32;
  if (REXCVAR_GET(wwe2k15_kbd_log)) {
    REXLOG_INFO("[kbd] button {} editor {:08X} lr {:08X} state[+3452]={} cursor[+3460]={}", ctx.r4.u32,
                editor, uint32_t(ctx.lr), Ld32(base, editor + 3452), Ld32(base, editor + 3460));
  }
  __imp__sub_828A6478(ctx, base);
}

extern "C" REX_FUNC(sub_828A2CA8) {
  if (REXCVAR_GET(wwe2k15_kbd_log)) {
    const char* s = reinterpret_cast<const char*>(base + ctx.r4.u32);
    REXLOG_INFO("[kbd] insert editor {:08X} text '{}' r5={} r6={} lr {:08X}", ctx.r3.u32,
                std::string(s, strnlen(s, 4)), ctx.r5.u32, ctx.r6.u32, uint32_t(ctx.lr));
  }
  __imp__sub_828A2CA8(ctx, base);
}

// Every text widget is drawn each frame by sub_82333D98(screen r3, widget r4) (found with a read
// trap on the text of a name field). The grid's text lines are widgets whose text pointer
// ([widget+0xA4]) is one of the line pointers the grid's editor keeps at editor+0xECC, and the
// screen they are drawn with is a member of the editor, 0x770 bytes in (editor A4BD5440, screen
// A4BD5BB0, 2026-10-27). That identifies the editor without any button press.
constexpr uint32_t kScreenInEditor = 0x770;
constexpr uint32_t kLinePointers = 0xECC;  // editor+: up to 8 line text pointers (line 0 first)
constexpr uint32_t kWidgetText = 0xA4;     // widget+: pointer to its text

// 0 when `screen` does not belong to a grid editor that owns this widget's text.
[[nodiscard]] uint32_t EditorOfLine(const uint8_t* base, uint32_t screen, uint32_t widget) {
  if (screen < kScreenInEditor || !widget) return 0;
  const uint32_t editor = screen - kScreenInEditor;
  const uint32_t text = Ld32(base, widget + kWidgetText);
  if (!text || !Ld32(base, editor + 44)) return 0;           // insert reads [editor+44]
  if (Ld32(base, editor + 3460) > 0x400) return 0;           // cursor
  for (uint32_t k = 0; k < 8; ++k)
    if (Ld32(base, editor + kLinePointers + 4 * k) == text) return editor;
  return 0;
}

extern "C" REX_FUNC(sub_82333D98) {
  const uint32_t screen = ctx.r3.u32, widget = ctx.r4.u32;
  __imp__sub_82333D98(ctx, base);
  if (!REXCVAR_GET(wwe2k15_kbd) || widget < 0x1000 || screen < 0x1000) return;
  const uint32_t editor = EditorOfLine(base, screen, widget);
  if (!editor) return;
  if (!GridOpen() || g_editor.load() != editor) {
    REXLOG_INFO("[kbd] letter grid open (editor {:08X})", editor);
    std::lock_guard lock(g_queue_mutex);
    g_queue.clear();  // keys typed before the grid opened are not for it
  }
  g_editor = editor;
  g_last_seen = NowMs();
  KeepTextInputOn();
  ApplyQueued(ctx, base, editor);
}
