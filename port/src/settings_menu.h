// wwe2k15 - in-game settings menu (F10, or Back + Start on a pad)
//
// A small ImGui window over the game for the players who run it without the launcher (Steam Deck,
// Linux / Proton) and for quick changes: the same options as the launcher, the same cvars. It is
// drawn only while open (the dialog exists only then, so a closed menu costs nothing), navigable
// with the keyboard (arrows, Enter, Esc) and with a pad (D-pad / left stick, A, B), and the game gets
// no input while it is open (the SDK's input "active" callback, see Wwe2k15SetMenuInputGate).
// Nothing is written unless "Apply and save" is chosen: the values go to the game's cvars (live ones
// at once), to the exe-side wwe2k15.toml, and, when a launcher is installed, to launcher-options.ini so
// the launcher does not put the old choice back (its command line wins over the toml). Items marked *
// take effect the next time the game starts.
//
// The game keeps running behind the window (there is no safe way to freeze the guest), so open it from
// the menus or from the pause screen of a match.
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging/macros.h>
#include <rex/ui/imgui_dialog.h>

inline std::atomic<bool> g_wwe2k15_menu_open{false};

namespace wwe2k15_menu {

struct Option {
  std::string text;
  std::vector<std::pair<std::string, std::string>> sets;  // cvar = value
};

struct Item {
  std::string label;
  std::vector<Option> opts;
  bool restart = false;       // takes effect at the next launch
  bool experimental = false;
  std::string ini_key;        // launcher-options.ini key ("" = none)
  std::function<std::string(const Option&)> ini_value;  // value written to the ini for an option
  int cur = 0;                // selected option (-1 = the current cvars match none)
  int orig = 0;
};

inline std::string Norm(std::string v) {
  std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\n')) v.pop_back();
  if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
  return v;
}

inline bool CvarExists(const std::string& name) { return !rex::cvar::GetFlagByName(name).empty(); }

inline bool OptionMatches(const Option& o) {
  for (const auto& [name, value] : o.sets)
    if (Norm(rex::cvar::GetFlagByName(name)) != Norm(value)) return false;
  return true;
}

inline std::vector<Item> BuildItems() {
  std::vector<Item> items;
  auto boolean = [](std::string label, const std::string& cvar, bool invert, bool restart, bool experimental,
                    std::string ini_key) {
    Item it;
    it.label = std::move(label);
    it.restart = restart;
    it.experimental = experimental;
    it.ini_key = std::move(ini_key);
    it.opts = {Option{"Off", {{cvar, invert ? "true" : "false"}}}, Option{"On", {{cvar, invert ? "false" : "true"}}}};
    it.ini_value = [](const Option& o) { return o.text == "On" ? std::string("1") : std::string("0"); };
    return it;
  };
  auto add = [&](Item it) {
    // Skip rows whose cvar does not exist in this build.
    for (const auto& o : it.opts)
      for (const auto& s : o.sets)
        if (!CvarExists(s.first)) return;
    it.cur = -1;
    for (size_t i = 0; i < it.opts.size(); ++i)
      if (OptionMatches(it.opts[i])) { it.cur = int(i); break; }
    it.orig = it.cur;
    items.push_back(std::move(it));
  };

  add(boolean("Full screen", "fullscreen", false, true, false, "fullscreen"));
#if defined(SVR_NATIVE_RENDERER)
  {
    Item it;
    it.label = "Resolution (native renderer)";
    it.restart = true;
    it.ini_key = "native_resolution";
    for (const char* r : {"0", "360", "480", "540", "720", "1080", "1440", "2160"})
      it.opts.push_back(Option{std::string(r) == "0" ? "Automatic" : std::string(r) + "p", {{"svr_render_scale", r}}});
    it.ini_value = [](const Option& o) { return o.sets[0].second; };
    add(std::move(it));
  }
  {
    Item it;
    it.label = "Anti-aliasing";
    it.restart = true;
    it.ini_key = "aa";
    it.opts = {Option{"Off", {{"svr_fxaa", "0"}}}, Option{"FXAA", {{"svr_fxaa", "1"}}},
               Option{"FXAA extreme", {{"svr_fxaa", "2"}}}};
    it.ini_value = [](const Option& o) { return o.sets[0].second; };
    add(std::move(it));
  }
#else
  {
    Item it;
    it.label = "Resolution";
    it.restart = true;
    it.ini_key = "resolution";
    it.opts = {Option{"720p", {{"resolution_scale", "1"}}}, Option{"1440p (x2)", {{"resolution_scale", "2"}}},
               Option{"4K (x3)", {{"resolution_scale", "3"}}}};
    it.ini_value = [](const Option& o) {
      const std::string& v = o.sets[0].second;
      return std::string(v == "3" ? "2160" : v == "2" ? "1440" : "720");
    };
    add(std::move(it));
  }
  add(boolean("Vertical sync", "vulkan_allow_present_mode_immediate", true, true, false, "vsync"));
  {
    Item it;
    it.label = "Anti-aliasing";
    it.restart = true;
    it.ini_key = "aa";
    it.opts = {Option{"Off", {{"swap_post_effect", "none"}}}, Option{"FXAA", {{"swap_post_effect", "fxaa"}}},
               Option{"FXAA extreme", {{"swap_post_effect", "fxaa_extreme"}}}};
    it.ini_value = [](const Option& o) {
      const std::string& v = o.sets[0].second;
      return std::string(v == "fxaa_extreme" ? "2" : v == "fxaa" ? "1" : "0");
    };
    add(std::move(it));
  }
#endif
  add(boolean("Entrances at 60 FPS", "wwe2k15_60fps", false, false, false, "entrances_60fps"));
  add(boolean("FPS counter", "wwe2k15_show_fps", false, true, false, "show_fps"));
  add(boolean("Camera cuts on finishers", "wwe2k15_camera_cuts_on_finishers", false, false, true, ""));
  add(boolean("Camera cuts on grapples", "wwe2k15_camera_cuts_on_holds", false, false, true, ""));
  add(boolean("Custom logo import (mods/logos/import.png)", "wwe2k15_logo_import", false, false, false, "logo_import"));
  add(boolean("Logo import: keep proportions", "wwe2k15_logo_import_keep_ratio", false, false, false, ""));
  {
    // One row sets both Universe experiments together (hidden if either cvar is missing). Read when a show is saved / a day
    // is played: no restart needed.
    Item it;
    it.label = "Universe: free shows (use a COPY of your save)";
    it.experimental = true;
    it.opts = {Option{"Off", {{"wwe2k15_universe_free_shows", "false"}, {"wwe2k15_universe_free_shows_play", "false"}}},
               Option{"On", {{"wwe2k15_universe_free_shows", "true"}, {"wwe2k15_universe_free_shows_play", "true"}}}};
    add(std::move(it));
  }
  return items;
}

// "key = value" lines of a toml file: replaces the value of an existing top-level key or inserts the key
// before the first [section]. Only the given keys are touched; comments and the rest stay as they are.
inline bool UpdateToml(const std::filesystem::path& path,
                       const std::vector<std::pair<std::string, std::string>>& kv) {
  std::vector<std::string> lines;
  {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  size_t first_section = lines.size();
  for (size_t i = 0; i < lines.size(); ++i) {
    const auto p = lines[i].find_first_not_of(" \t");
    if (p != std::string::npos && lines[i][p] == '[') { first_section = i; break; }
  }
  for (const auto& [key, value] : kv) {
    bool done = false;
    for (size_t i = 0; i < first_section; ++i) {
      const auto p = lines[i].find_first_not_of(" \t");
      if (p == std::string::npos || lines[i][p] == '#') continue;
      const auto eq = lines[i].find('=', p);
      if (eq == std::string::npos) continue;
      std::string k = lines[i].substr(p, eq - p);
      while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
      if (k == key) { lines[i] = key + " = " + value; done = true; break; }
    }
    if (!done) {
      lines.insert(lines.begin() + std::ptrdiff_t(first_section), key + " = " + value);
      ++first_section;
    }
  }
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  for (const auto& l : lines) out << l << "\n";
  return bool(out);
}

// launcher-options.ini is "key=value" per line: only the given keys are replaced or appended.
inline void UpdateLauncherIni(const std::filesystem::path& path,
                              const std::vector<std::pair<std::string, std::string>>& kv) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return;
  std::vector<std::string> lines;
  {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  for (const auto& [key, value] : kv) {
    bool done = false;
    for (auto& l : lines) {
      if (l.rfind(key + "=", 0) == 0) { l = key + "=" + value; done = true; break; }
    }
    if (!done) lines.push_back(key + "=" + value);
  }
  std::ofstream out(path, std::ios::trunc);
  for (const auto& l : lines) out << l << "\n";
}

inline std::string TomlValue(const std::string& v) {
  const std::string n = Norm(v);
  if (n == "true" || n == "false") return n;
  char* end = nullptr;
  std::strtod(n.c_str(), &end);
  if (!n.empty() && end && *end == '\0') return n;
  return "\"" + v + "\"";
}

#if defined(_WIN32)
struct XState { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
inline DWORD (WINAPI* g_xinput_get)(DWORD, XState*) = nullptr;
inline bool PollPad(WORD& buttons, float& lx, float& ly) {
  static bool tried = false;
  if (!tried) {
    tried = true;
    for (const wchar_t* dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
      if (HMODULE m = LoadLibraryW(dll)) {
        g_xinput_get = reinterpret_cast<decltype(g_xinput_get)>(GetProcAddress(m, "XInputGetState"));
        if (g_xinput_get) break;
      }
    }
  }
  if (!g_xinput_get) return false;
  bool any = false;
  buttons = 0;
  lx = ly = 0;
  for (DWORD i = 0; i < 4; ++i) {
    XState s{};
    if (g_xinput_get(i, &s) == ERROR_SUCCESS) {
      any = true;
      buttons |= s.buttons;
      if (std::fabs(float(s.lx)) > std::fabs(lx * 32767.f)) lx = float(s.lx) / 32767.f;
      if (std::fabs(float(s.ly)) > std::fabs(ly * 32767.f)) ly = float(s.ly) / 32767.f;
    }
  }
  return any;
}
constexpr WORD kPadUp = 0x0001, kPadDown = 0x0002, kPadLeft = 0x0004, kPadRight = 0x0008, kPadStart = 0x0010,
               kPadBack = 0x0020, kPadLB = 0x0100, kPadRB = 0x0200, kPadA = 0x1000, kPadB = 0x2000;
#endif

class Dialog : public rex::ui::ImGuiDialog {
 public:
  Dialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path toml_path, std::function<void()> close,
         std::function<void()> quit_game = nullptr)
      : rex::ui::ImGuiDialog(drawer),
        toml_path_(std::move(toml_path)),
        close_(std::move(close)),
        quit_game_(std::move(quit_game)),
        items_(BuildItems()) {
    auto& io = GetIO();
    saved_nav_flags_ = io.ConfigFlags;
    saved_backend_flags_ = io.BackendFlags;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  }
  ~Dialog() override {
    auto& io = GetIO();
    io.ConfigFlags = saved_nav_flags_;
    io.BackendFlags = saved_backend_flags_;
    // Release the pad keys we may have held down.
    for (ImGuiKey k : {ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadDpadLeft,
                       ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight})
      io.AddKeyEvent(k, false);
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    FeedPad(io);
    // (F10 is a global bind that toggles the menu, see wwe2k15_app.h.)
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      if (frames_ > 2) { close_(); return; }
    }
    ++frames_;
    const float scale = std::clamp(io.DisplaySize.y / 540.0f, 1.3f, 2.6f);  // bigger on small screens (Deck, 720p)
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x * 0.92f, 760.0f * scale), 0));
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::Begin("WWE 2K15 PC - Settings##wwe2k15_settings", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(scale);
    ImGui::TextDisabled("Up/Down: choose   Left/Right or A: change   B / Esc: cancel   * = next launch");
    ImGui::Separator();
    bool any_changed = false;
    for (size_t i = 0; i < items_.size(); ++i) {
      Item& it = items_[i];
      ImGui::PushID(int(i));
      const char* value = it.cur >= 0 ? it.opts[size_t(it.cur)].text.c_str() : "(custom)";
      char row[256];
      std::snprintf(row, sizeof row, "%s%s%s", it.label.c_str(), it.restart ? " *" : "",
                    it.experimental ? " (experimental)" : "");
      const float avail = ImGui::GetContentRegionAvail().x;
      if (ImGui::Selectable(row, false, 0, ImVec2(avail, 0))) Step(it, +1);
      if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight))
          Step(it, +1);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft))
          Step(it, -1);
      }
      ImGui::SameLine(avail - ImGui::CalcTextSize(value).x - 8.0f * scale);
      ImGui::TextColored(it.cur != it.orig ? ImVec4(0.95f, 0.8f, 0.3f, 1) : ImVec4(0.8f, 0.9f, 1, 1), "%s", value);
      any_changed |= it.cur != it.orig;
      ImGui::PopID();
    }
    ImGui::Separator();
    if (!applied_msg_.empty()) ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1), "%s", applied_msg_.c_str());
    if (ImGui::Button("Apply and save")) Apply();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) { close_(); ImGui::End(); return; }
    if (quit_game_) {
      ImGui::SameLine();
      if (ImGui::Button("Quit game")) confirm_quit_ = true;
    }
    if (confirm_quit_) {
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.95f, 0.6f, 0.3f, 1), "Quit the game? Anything not saved in the game is lost.");
      if (ImGui::Button("Yes, quit")) { confirm_quit_ = false; quit_game_(); ImGui::End(); return; }
      ImGui::SameLine();
      if (ImGui::Button("No")) confirm_quit_ = false;
    }
    ImGui::TextDisabled("The game keeps running behind this window: open it from the menus or the pause screen.");
    if (any_changed) ImGui::TextDisabled("Nothing is saved until \"Apply and save\".");
    if (!focused_once_) { ImGui::SetKeyboardFocusHere(-100); focused_once_ = true; }
    ImGui::End();
  }

 private:
  void Step(Item& it, int dir) {
    const int n = int(it.opts.size());
    if (n == 0) return;
    int c = it.cur < 0 ? (dir > 0 ? 0 : n - 1) : (it.cur + dir + n) % n;
    it.cur = c;
    applied_msg_.clear();
  }

  void FeedPad(ImGuiIO& io) {
#if defined(_WIN32)
    WORD b = 0;
    float lx = 0, ly = 0;
    const bool have = PollPad(b, lx, ly);
    const bool up = have && ((b & kPadUp) || ly > 0.55f), down = have && ((b & kPadDown) || ly < -0.55f);
    const bool left = have && ((b & kPadLeft) || lx < -0.55f), right = have && ((b & kPadRight) || lx > 0.55f);
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, up);
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, down);
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, left);
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, right);
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, have && (b & kPadA));
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, have && (b & kPadB));
#else
    (void)io;
#endif
  }

  void Apply() {
    std::vector<std::pair<std::string, std::string>> toml, ini;
    int changed = 0;
    for (auto& it : items_) {
      if (it.cur < 0 || it.cur == it.orig) continue;
      ++changed;
      const Option& o = it.opts[size_t(it.cur)];
      for (const auto& [name, value] : o.sets) {
        if (!it.restart) rex::cvar::SetFlagByName(name, value);
        toml.emplace_back(name, TomlValue(value));
      }
      if (!it.ini_key.empty() && it.ini_value) ini.emplace_back(it.ini_key, it.ini_value(o));
      it.orig = it.cur;
    }
    if (!changed) { applied_msg_ = "No change."; return; }
    const bool ok = UpdateToml(toml_path_, toml);
    UpdateLauncherIni(rex::filesystem::GetExecutableFolder() / "launcher-options.ini", ini);
    applied_msg_ = ok ? "Saved. * items apply at the next launch." : "Could not write wwe2k15.toml.";
    REXLOG_INFO("settings menu: {} option(s) changed, {}", changed, ok ? "saved to wwe2k15.toml" : "toml write failed");
  }

  std::filesystem::path toml_path_;
  std::function<void()> close_;
  std::function<void()> quit_game_;
  bool confirm_quit_ = false;
  std::vector<Item> items_;
  ImGuiConfigFlags saved_nav_flags_ = 0;
  ImGuiBackendFlags saved_backend_flags_ = 0;
  std::string applied_msg_;
  int frames_ = 0;
  bool focused_once_ = false;
};

}  // namespace wwe2k15_menu
