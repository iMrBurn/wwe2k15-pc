// WWE 2K15 PC: type with the physical keyboard in the game's text screens (CAW names, Create a
// Story text, titles...). SKELETON, not built yet (not in CMakeLists.txt): the hook points marked
// TODO are found with the test plan in docs/PORTING_PLAN.md ("Keyboard text entry: test plan").
//
// Shape, taken from WWE '12 PC (wwe12-pc/port/src/native/wwe12_kbd.cpp, same GPL-3.0 family):
//   - a window input listener at z 1000 (before mnk_mode) turns OnKeyChar text into a queue of
//     characters while a text screen is open, and marks the typing keys handled so they do not
//     also drive the pad (Z, Backspace...);
//   - the game's own thread replays the queue through the game's insert / back functions inside
//     a hook of the widget draw function, exactly as if the on-screen buttons were pressed.
// Two cases are possible in 2K15 and the first test decides which one applies:
//   A) the screen calls XamShowKeyboardUI (sub_83389A78): then the SDK's ImGui dialog already
//      takes the physical keyboard and nothing is needed here except making sure it has focus;
//   B) the game has its own letter grid like WWE '12: the TODO hooks below are needed.
// Disable with wwe2k15_kbd = false in wwe2k15.toml. Works in both exes (no renderer code).

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

REXCVAR_DEFINE_BOOL(wwe2k15_kbd, true, "WWE2K15",
                    "Type with the physical keyboard in the game's text screens.");
REXCVAR_DEFINE_BOOL(wwe2k15_kbd_log, false, "WWE2K15",
                    "Log the text screen's editor and every key typed (probe).");

namespace {

constexpr char kBackspace = '\b';
constexpr long long kScreenTimeoutMs = 400;  // seen less than this long ago = still open

rex::ui::Window* g_window = nullptr;
std::atomic<uint32_t> g_editor{0};          // guest address of the text editor object (TODO: find)
std::atomic<long long> g_last_seen{0};      // ms, steady clock

std::mutex g_queue_mutex;
std::deque<char> g_queue;  // typed characters (or kBackspace) waiting for the game thread

[[nodiscard]] long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

[[nodiscard]] bool TextScreenOpen() {
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

// Keys that type while a text screen is open (kept away from mnk_mode). Win32 virtual-key codes.
[[nodiscard]] bool IsTypingKey(uint16_t vk) {
  return vk == 0x08 /*Backspace*/ || vk == 0x20 /*Space*/ || (vk >= '0' && vk <= '9') ||
         (vk >= 'A' && vk <= 'Z') || (vk >= 0x60 && vk <= 0x6F) /*numpad*/ ||
         (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2 /*OEM*/;
}

class KbdListener final : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (!TextScreenOpen() || e.is_ctrl_pressed()) return;
    const auto vk = static_cast<uint16_t>(e.virtual_key());
    if (!IsTypingKey(vk)) return;
    if (vk == 0x08) Push(kBackspace);
    e.set_handled(true);
  }

  void OnKeyChar(rex::ui::KeyEvent& e) override {
    if (!TextScreenOpen()) return;
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
// text boxes close, so it is asked again whenever a text screen is seen with it off.
void KeepTextInputOn() {
  rex::ui::Window* window = g_window;
  if (!window || window->IsTextInputActive()) return;
  window->app_context().CallInUIThread([window] { window->SetTextInputActive(true); });
}

}  // namespace

// Call once the window exists (Wwe2k15App::OnPreLaunchModule, next to the renderer's setup).
void Wwe2k15KbdInit(rex::ui::Window* window) {
  g_window = window;
  if (window) window->AddInputListener(&g_listener, 1000);  // before mnk_mode's listener
}

// TODO(hook 1, widget draw): REX_FUNC override of the game's text-widget draw function (the
// WWE '12 equivalent is sub_824C9228(screen r3, widget r4)). After the original runs, when the
// widget is a text-screen line (name starts with "_write" in WWE '12), derive the editor from the
// screen, set g_editor / g_last_seen, call KeepTextInputOn(), then ApplyQueued().
//
// TODO(ApplyQueued): swap g_queue, then for each char call the game's insert function
// (WWE '12: sub_828A47D0(editor, const char* s, 1, 0), s in guest memory on a scratch stack
// area) or, for kBackspace, the game's BACK function (WWE '12: sub_828A6158(editor)).
//
// TODO(hook 2, optional probes): REX_FUNC overrides of insert / back that log when
// wwe2k15_kbd_log is on (caller LR, editor, text) to confirm the editor is the same object.
