#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "i18n.h"
#include "stfs.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> target;
UINT resize_w = 0, resize_h = 0;
fs::path root, user_data, game_data;
std::string page = "JOUER", status;
HANDLE game_process = nullptr;
bool disable_obs_vulkan_hook = true;

// Game options passed to wwe2k15.exe as cvars (saved in launcher-options.ini).
struct Options {
  bool fullscreen = true;
  int resolution_scale = 1;   // resolution_scale (1-4)
  int window_size = 2;        // windowed size index into kWindowSizes
  int monitor = 0;            // monitor: 0 = default, 1..3
  int aa = 0;                 // swap_post_effect: 0 none, 1 fxaa, 2 fxaa_extreme
  bool letterbox = true;      // present_letterbox
  int anisotropic = 3;        // anisotropic_override: 0 off, 2 = 2x, 3 = 4x, 4 = 8x, 5 = 16x
  bool fsi = true;            // render_target_path_vulkan = fsi (fixes entrance black screens)
  bool keyboard = false;      // mnk_mode
  bool azerty = true;         // French keyboard layout for the keyboard pad
  int language = 0;           // user_language (XLanguage): 1 en, 3 de, 4 fr, 5 es, 6 it; 0 = unset
  bool cpu_yield = false;     // wwe2k15_render_wait_yield (experimental)
  bool show_fps = false;      // wwe2k15_show_fps
} opt;

const char* tr(const char* fr) { return i18n::Translate(fr, opt.language); }

std::wstring Widen(const char* utf8) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  std::wstring w(n > 0 ? n - 1 : 0, L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w.data(), n);
  return w;
}
ImFont* logo_font = nullptr;

void MakeTarget() {
  ComPtr<ID3D11Texture2D> back;
  if (SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&back))))
    device->CreateRenderTargetView(back.Get(), nullptr, &target);
}

bool MakeDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
      levels, 2, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context);
  if (FAILED(hr))
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context);
  if (FAILED(hr)) return false;
  MakeTarget();
  return true;
}

fs::path ExeDir() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return fs::path(path).parent_path();
}

void LoadOptions();

void LoadSettings() {
  wchar_t local[MAX_PATH]{};
  SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local);
  user_data = fs::path(local) / L"WWE2K15";
  fs::create_directories(user_data);
  const fs::path cfg = root / L"launcher-settings.txt";
  std::error_code ec;
  if (fs::exists(cfg, ec)) {
    std::wstring line;
    std::wifstream in(cfg);
    std::getline(in, line);
    if (!line.empty() && fs::exists(fs::path(line) / L"default.xex", ec)) game_data = line;
    std::getline(in, line);
    if (line == L"0") disable_obs_vulkan_hook = false;
    else if (line == L"1") disable_obs_vulkan_hook = true;
  }
  wchar_t desktop[MAX_PATH]{};
  SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, desktop);
  for (const auto& p : {root / L"assets", root / L"game", root.parent_path() / L"assets",
                         fs::path(desktop) / L"WWE2K15"}) {
    if (game_data.empty() && fs::exists(p / L"default.xex", ec)) game_data = p;
  }
  LoadOptions();
}

bool HasGame() {
  std::error_code ec;
  return !game_data.empty() && fs::exists(game_data / L"default.xex", ec);
}

// The game is generated from Title Update 4: default.xexp must sit beside default.xex.
bool HasTitleUpdate() {
  std::error_code ec;
  return HasGame() && fs::exists(game_data / L"default.xexp", ec);
}

int DlcCount() {
  std::error_code ec;
  int count = 0;
  if (HasGame())
    for (const auto& e : fs::directory_iterator(game_data / L"dlc", ec))
      if (e.is_regular_file(ec)) ++count;
  return count;
}

constexpr uint32_t kTitleId = 0x545408B6;

// Installs one STFS package: a WWE 2K15 title update is extracted into the game folder
// (default.xexp beside default.xex, the rest under update\); a DLC package is copied
// unchanged into dlc\, where the game installs it on its next start.
bool InstallPackage(const fs::path& file, std::string& msg) {
  const std::string name = file.filename().string();
  stfs::Package pkg;
  if (!pkg.Open(file)) { msg = name + tr(" : pas un package Xbox 360 (STFS) lisible."); return false; }
  if (pkg.title_id != kTitleId) { msg = name + tr(" : ce package n'est pas pour WWE 2K15."); return false; }
  std::error_code ec;
  if (pkg.content_type == stfs::kTypeMarketplace) {
    fs::create_directories(game_data / L"dlc", ec);
    fs::copy_file(file, game_data / L"dlc" / file.filename(), fs::copy_options::overwrite_existing, ec);
    msg = ec ? name + tr(" : copie impossible (") + ec.message() + ")."
             : tr("DLC ajouté : ") + pkg.display_name + ".";
    return !ec;
  }
  if (pkg.content_type != stfs::kTypeTitleUpdate) { msg = name + tr(" : type de package non pris en charge."); return false; }
  if (pkg.version != 4) { msg = name + tr(" : seule la Title Update 4 est prise en charge."); return false; }
  for (size_t i = 0; i < pkg.entries.size(); ++i) {
    const auto& entry = pkg.entries[i];
    const std::string rel = pkg.PathOf(i);
    const fs::path dst = rel == "default.xexp" ? game_data / L"default.xexp"
                                               : game_data / L"update" / fs::path(rel);
    if (entry.directory) { fs::create_directories(dst, ec); continue; }
    fs::create_directories(dst.parent_path(), ec);
    if (!pkg.Extract(entry, dst)) { msg = name + tr(" : extraction échouée pour ") + rel + "."; return false; }
  }
  msg = tr("Title Update 4 installée.");
  return true;
}

void AddPackages() {
  if (!HasGame()) { status = tr("Sélectionne d'abord le dossier qui contient default.xex."); return; }
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
  dialog->SetTitle(Widen(tr("Choisis la Title Update (tu0000...) et/ou les DLC de WWE 2K15")).c_str());
  if (SUCCEEDED(dialog->Show(nullptr))) {
    IShellItemArray* items = nullptr;
    if (SUCCEEDED(dialog->GetResults(&items))) {
      DWORD count = 0;
      items->GetCount(&count);
      std::string last;
      int ok = 0;
      for (DWORD i = 0; i < count; ++i) {
        IShellItem* item = nullptr;
        PWSTR path = nullptr;
        if (SUCCEEDED(items->GetItemAt(i, &item)) &&
            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
          if (InstallPackage(path, last)) ++ok;
          CoTaskMemFree(path);
        }
        if (item) item->Release();
      }
      items->Release();
      status = count == 1 ? last
                          : std::to_string(ok) + "/" + std::to_string(count) +
                                tr(" package(s) installé(s). Dernier : ") + last;
    }
  }
  dialog->Release();
}

void SaveOptions() {
  std::ofstream out(root / L"launcher-options.ini", std::ios::trunc);
  if (!out) return;
  out << "fullscreen=" << opt.fullscreen << "\nresolution_scale=" << opt.resolution_scale
      << "\nanisotropic=" << opt.anisotropic << "\nfsi=" << opt.fsi
      << "\nlanguage=" << opt.language << "\nkeyboard=" << opt.keyboard << "\nazerty=" << opt.azerty << "\ncpu_yield=" << opt.cpu_yield << "\nshow_fps=" << opt.show_fps << "\nwindow_size=" << opt.window_size << "\nmonitor=" << opt.monitor << "\naa=" << opt.aa << "\nletterbox=" << opt.letterbox << "\n";
}

void LoadOptions() {
  std::ifstream in(root / L"launcher-options.ini");
  std::string line;
  while (std::getline(in, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq);
    const int v = std::atoi(line.c_str() + eq + 1);
    if (k == "fullscreen") opt.fullscreen = v != 0;
    else if (k == "resolution_scale") opt.resolution_scale = std::clamp(v, 1, 4);
    else if (k == "window_size") opt.window_size = std::clamp(v, 0, 4);
    else if (k == "monitor") opt.monitor = std::clamp(v, 0, 3);
    else if (k == "aa") opt.aa = std::clamp(v, 0, 2);
    else if (k == "letterbox") opt.letterbox = v != 0;
    else if (k == "anisotropic") opt.anisotropic = std::clamp(v, 0, 5);
    else if (k == "fsi") opt.fsi = v != 0;
    else if (k == "keyboard") opt.keyboard = v != 0;
    else if (k == "azerty") opt.azerty = v != 0;
    else if (k == "language") opt.language = v;
    else if (k == "cpu_yield") opt.cpu_yield = v != 0;
    else if (k == "show_fps") opt.show_fps = v != 0;
  }
  if (opt.language != 1 && opt.language != 3 && opt.language != 4 && opt.language != 5 &&
      opt.language != 6) {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
      case LANG_FRENCH: opt.language = 4; break;
      case LANG_GERMAN: opt.language = 3; break;
      case LANG_SPANISH: opt.language = 5; break;
      case LANG_ITALIAN: opt.language = 6; break;
      default: opt.language = 1; break;
    }
  }
}

struct WindowSize { int w, h; const char* label; };
constexpr WindowSize kWindowSizes[] = {
    {1280, 720, "1280x720"}, {1600, 900, "1600x900"}, {1920, 1080, "1920x1080"},
    {2560, 1440, "2560x1440"}, {3840, 2160, "3840x2160"}};

std::wstring OptionArgs() {
  auto b = [](bool v) { return std::wstring(v ? L"true" : L"false"); };
  return L" --fullscreen=" + b(opt.fullscreen) +
         L" --resolution_scale=" + std::to_wstring(opt.resolution_scale) +
         L" --anisotropic_override=" + std::to_wstring(opt.anisotropic) +
         L" --render_target_path_vulkan=" + (opt.fsi ? L"fsi" : L"rtv") +
         L" --mnk_mode=" + b(opt.keyboard) +
         L" --user_language=" + std::to_wstring(opt.language) +
         L" --wwe2k15_render_wait_yield=" + b(opt.cpu_yield) +
         L" --wwe2k15_show_fps=" + b(opt.show_fps) +
         L" --window_width=" + std::to_wstring(kWindowSizes[opt.window_size].w) +
         L" --window_height=" + std::to_wstring(kWindowSizes[opt.window_size].h) +
         L" --monitor=" + std::to_wstring(opt.monitor) +
         L" --swap_post_effect=" + (opt.aa == 2 ? L"fxaa_extreme" : opt.aa == 1 ? L"fxaa" : L"none") +
         L" --present_letterbox=" + b(opt.letterbox) +
         // The keyboard pad reads layout-dependent virtual keys: on AZERTY move the left stick
         // to Z Q S D and the keys it displaces (LT on Q, Back on Z) elsewhere.
         (opt.keyboard && opt.azerty
              ? std::wstring(L" --keybind_lstick_up=Z --keybind_lstick_left=Q --keybind_lstick_down=S"
                             L" --keybind_lstick_right=D --keybind_left_trigger=A,I"
                             L" --keybind_back=W,Tab")
              : std::wstring());
}

void SaveSettings() {
  std::wofstream out(root / L"launcher-settings.txt", std::ios::trunc);
  if (out) out << game_data.wstring() << L"\n" << (disable_obs_vulkan_hook ? L"1" : L"0");
  SaveOptions();
}

void OpenFolder(const fs::path& p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void BrowseGame() {
  IFileDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
  dialog->SetTitle(Widen(tr("Choisis le dossier contenant default.xex")).c_str());
  if (SUCCEEDED(dialog->Show(nullptr))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        fs::path selected(path);
        if (fs::exists(selected / L"default.xex")) {
          game_data = selected;
          SaveSettings();
          status = tr("Dossier du jeu enregistré.");
        } else status = tr("Ce dossier ne contient pas default.xex.");
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
}

void LaunchGame() {
  if (game_process && WaitForSingleObject(game_process, 0) == WAIT_TIMEOUT) {
    status = tr("Le jeu tourne déjà.");
    return;
  }
  if (game_process) { CloseHandle(game_process); game_process = nullptr; }
  const fs::path exe = root / L"wwe2k15.exe";
  if (!fs::exists(exe)) { status = tr("wwe2k15.exe est introuvable : reconstruis le projet."); return; }
  if (!HasGame()) { status = tr("Sélectionne le dossier qui contient default.xex."); page = "RÉGLAGES"; return; }
  if (!HasTitleUpdate()) { status = tr("Ajoute la Title Update 4 (RÉGLAGES > AJOUTER TU / DLC)."); page = "RÉGLAGES"; return; }
  SaveSettings();
  const std::wstring args = L"--game_data_root=\"" + game_data.wstring() +
      L"\" --user_data_root=\"" + user_data.wstring() +
      L"\" --gpu_plugin=xenos --gpu_backend=vulkan --input_backend=sdl --hid_mappings_file=\"" +
      (root / L"gamecontrollerdb.txt").wstring() + L"\"" + OptionArgs();
  SHELLEXECUTEINFOW info{sizeof(info)};
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpFile = exe.c_str();
  info.lpParameters = args.c_str();
  info.lpDirectory = root.c_str();
  info.nShow = SW_SHOWNORMAL;
  constexpr wchar_t obs_layer_disable_var[] = L"DISABLE_VULKAN_OBS_CAPTURE";
  const DWORD old_value_size = GetEnvironmentVariableW(obs_layer_disable_var, nullptr, 0);
  std::wstring old_value;
  if (old_value_size > 0) {
    old_value.resize(old_value_size);
    const DWORD chars = GetEnvironmentVariableW(obs_layer_disable_var, old_value.data(), old_value_size);
    if (chars > 0 && chars < old_value_size) old_value.resize(chars);
    else old_value.clear();
  }
  const bool env_set = SetEnvironmentVariableW(
      obs_layer_disable_var, disable_obs_vulkan_hook ? L"1" : nullptr) != FALSE;
  const BOOL launched = env_set && ShellExecuteExW(&info);
  if (old_value_size > 0 && !old_value.empty()) {
    SetEnvironmentVariableW(obs_layer_disable_var, old_value.c_str());
  } else {
    SetEnvironmentVariableW(obs_layer_disable_var, nullptr);
  }
  if (launched) {
    game_process = info.hProcess;
    status = game_process ? tr("Jeu en cours de démarrage…") : tr("Commande de lancement envoyée.");
  } else if (!env_set) {
    status = tr("Impossible de régler l'option du hook OBS Vulkan.");
  } else status = tr("Windows n'a pas pu démarrer le jeu.");
}

void UpdateGameProcess() {
  if (!game_process) return;
  if (WaitForSingleObject(game_process, 0) == WAIT_TIMEOUT) {
    status = tr("Jeu en cours d'exécution.");
    return;
  }
  DWORD exit_code = 0;
  GetExitCodeProcess(game_process, &exit_code);
  CloseHandle(game_process);
  game_process = nullptr;
  status = tr("Le jeu s'est arrêté (code ") + std::to_string(exit_code) + tr("). Consulte JOURNAUX.");
}

// SDL3 gamepad input (DualSense, Xbox, ...) fed into ImGui navigation; ImGui's own backend is XInput-only.
SDL_Gamepad* pad = nullptr;
void InitPad() {
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (!SDL_Init(SDL_INIT_GAMEPAD)) return;
  SDL_AddGamepadMappingsFromFile((root / L"gamecontrollerdb.txt").string().c_str());
}
void PollPad() {
  SDL_UpdateGamepads();
  if (pad && !SDL_GamepadConnected(pad)) { SDL_CloseGamepad(pad); pad = nullptr; }
  if (!pad) {
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    if (ids && n > 0) pad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
  }
  if (!pad) return;
  ImGuiIO& io = ImGui::GetIO();
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  struct Map { SDL_GamepadButton b; ImGuiKey k; };
  static const Map map[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, ImGuiKey_GamepadFaceDown}, {SDL_GAMEPAD_BUTTON_EAST, ImGuiKey_GamepadFaceRight},
    {SDL_GAMEPAD_BUTTON_WEST, ImGuiKey_GamepadFaceLeft}, {SDL_GAMEPAD_BUTTON_NORTH, ImGuiKey_GamepadFaceUp},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ImGuiKey_GamepadL1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ImGuiKey_GamepadR1},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, ImGuiKey_GamepadDpadUp}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, ImGuiKey_GamepadDpadDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, ImGuiKey_GamepadDpadLeft}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, ImGuiKey_GamepadDpadRight},
  };
  for (const Map& m : map) io.AddKeyEvent(m.k, SDL_GetGamepadButton(pad, m.b));
}

// ---- UI: inspired by the WWE 2K15 menus (charcoal background, white highlight, angled tiles) ----
constexpr ImU32 kWhite = IM_COL32(244, 244, 246, 255);
constexpr ImU32 kInk = IM_COL32(14, 14, 16, 255);
constexpr ImU32 kRed = IM_COL32(200, 28, 46, 255);
constexpr ImU32 kGold = IM_COL32(214, 176, 74, 255);
constexpr ImU32 kPanel = IM_COL32(24, 25, 28, 235);
constexpr ImU32 kMuted = IM_COL32(150, 152, 158, 255);
ImFont* ui_font = nullptr;
ImFont* title_font = nullptr;
const char* kTabs[] = {"JOUER", "RÉGLAGES", "GRAPHISMES", "CONTRÔLES", "MODS", "SAUVEGARDES", "À PROPOS"};
constexpr int kTabCount = 7;

int TabIndex() {
  for (int i = 0; i < kTabCount; ++i) if (page == kTabs[i]) return i;
  return 0;
}

float TextW(ImFont* f, float size, const char* s) { return f->CalcTextSizeA(size, FLT_MAX, 0, s).x; }

void Parallelogram(ImDrawList* d, ImVec2 a, ImVec2 b, float skew, ImU32 col) {
  ImVec2 pts[] = {{a.x + skew, a.y}, {b.x, a.y}, {b.x - skew, b.y}, {a.x, b.y}};
  d->AddConvexPolyFilled(pts, 4, col);
}

// Angled tile button. Returns true when clicked.
bool Tile(const char* id, const char* label, ImVec2 size, bool selected, bool primary = false, float font_size = 20) {
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();
  ImDrawList* d = ImGui::GetWindowDrawList();
  const float skew = size.y * 0.28f;
  ImU32 fill = IM_COL32(34, 35, 40, 255), text = kWhite;
  if (primary) { fill = hovered || selected ? kWhite : kRed; text = hovered || selected ? kInk : kWhite; }
  else if (selected || hovered) { fill = kWhite; text = kInk; }
  if (held) fill = kGold;
  Parallelogram(d, {p.x + 3, p.y + 4}, {p.x + size.x + 3, p.y + size.y + 4}, skew, IM_COL32(0, 0, 0, 120));
  Parallelogram(d, p, {p.x + size.x, p.y + size.y}, skew, fill);
  const ImVec2 t = ui_font->CalcTextSizeA(font_size, FLT_MAX, 0, label);
  d->AddText(ui_font, font_size, {p.x + (size.x - t.x) * 0.5f, p.y + (size.y - t.y) * 0.5f}, text, label);
  return ImGui::IsItemClicked();
}

void SectionTitle(ImDrawList* d, ImVec2 p, const char* title, const char* sub) {
  d->AddRectFilled({p.x, p.y + 4}, {p.x + 6, p.y + 40}, kRed);
  d->AddText(title_font, 40, {p.x + 18, p.y - 2}, kWhite, title);
  if (sub) d->AddText(ui_font, 17, {p.x + 20, p.y + 48}, kMuted, sub);
}

void Panel(ImDrawList* d, ImVec2 a, ImVec2 b) {
  d->AddRectFilled(a, b, kPanel);
  d->AddRectFilled(a, {b.x, a.y + 3}, IM_COL32(255, 255, 255, 40));
}

void Chip(ImDrawList* d, ImVec2 p, const char* k, const char* v, ImU32 accent) {
  d->AddRectFilled(p, {p.x + 4, p.y + 46}, accent);
  d->AddText(ui_font, 14, {p.x + 14, p.y + 2}, kMuted, k);
  d->AddText(ui_font, 22, {p.x + 14, p.y + 19}, kWhite, v);
}

void DrawBackground(ImDrawList* d, ImVec2 size, float t) {
  d->AddRectFilledMultiColor({0, 0}, size, IM_COL32(30, 31, 36, 255), IM_COL32(24, 25, 29, 255),
                             IM_COL32(6, 6, 8, 255), IM_COL32(8, 8, 10, 255));
  // soft diagonal light streaks, like the 2K15 menu backdrop
  const float drift = sinf(t * 0.25f) * 24.0f;
  for (int i = 0; i < 4; ++i) {
    const float x = size.x * (0.30f + 0.17f * i) + drift * (i % 2 ? -1 : 1);
    const float w = 70.0f + 38.0f * i;
    ImVec2 pts[] = {{x, 0}, {x + w, 0}, {x + w - size.y * 0.55f, size.y}, {x - size.y * 0.55f, size.y}};
    d->AddConvexPolyFilled(pts, 4, IM_COL32(255, 255, 255, 5 + 2 * i));
  }
  d->AddRectFilledMultiColor({0, size.y - 220}, size, 0, 0, IM_COL32(0, 0, 0, 230), IM_COL32(0, 0, 0, 230));
  Parallelogram(d, {-20, size.y - 58}, {420, size.y - 50}, 8, kRed);
  Parallelogram(d, {-20, size.y - 48}, {330, size.y - 44}, 6, kGold);
}

void DrawHeader(ImDrawList* d) {
  d->AddText(title_font, 34, {44, 16}, kWhite, "WWE");
  d->AddText(title_font, 34, {112, 16}, kRed, "2K15");
  d->AddText(ui_font, 13, {46, 58}, kMuted, tr("PC  ·  RECOMPILATION NATIVE"));
  float x = 360;
  const float y = 24, h = 44;
  const int sel = TabIndex();
  d->AddText(ui_font, 15, {x - 36, y + 13}, kMuted, "LB");
  for (int i = 0; i < kTabCount; ++i) {
    const float w = 28.0f + TextW(ui_font, 17, tr(kTabs[i]));
    ImGui::SetCursorScreenPos({x, y});
    if (Tile(kTabs[i], tr(kTabs[i]), {w, h}, sel == i, false, 17)) page = kTabs[i];
    x += w + 6;
  }
  d->AddText(ui_font, 15, {x + 14, y + 13}, kMuted, "RB");
}

void DrawFooter(ImDrawList* d, ImVec2 size) {
  d->AddRectFilled({0, size.y - 44}, size, IM_COL32(0, 0, 0, 215));
  d->AddRectFilled({0, size.y - 44}, {size.x, size.y - 43}, IM_COL32(255, 255, 255, 30));
  auto hint = [&](float x, const char* key, const char* what) {
    const float kw = TextW(ui_font, 15, key);
    d->AddRectFilled({x, size.y - 34}, {x + kw + 16, size.y - 12}, kWhite, 3);
    d->AddText(ui_font, 15, {x + 8, size.y - 32}, kInk, key);
    d->AddText(ui_font, 16, {x + kw + 26, size.y - 33}, kWhite, what);
    return x + kw + 26 + TextW(ui_font, 16, what) + 28;
  };
  float x = 44;
  x = hint(x, "Q / E  ·  LB / RB", tr("Changer d'onglet"));
  hint(x, tr("ÉCHAP / B"), tr("Retour à Jouer"));
#ifndef WWE2K15_VERSION
#define WWE2K15_VERSION "dev"
#endif
  const std::string ver_text = std::string("v" WWE2K15_VERSION "  ·  ") + tr("Beta de développement");
  const char* ver = ver_text.c_str();
  d->AddText(ui_font, 15, {size.x - TextW(ui_font, 15, ver) - 44, size.y - 32}, kMuted, ver);
}

void DrawPlay(ImDrawList* d, ImVec2 o, ImVec2 area) {
  UpdateGameProcess();
  Panel(d, o, {o.x + area.x, o.y + area.y});
  const bool has = HasGame();
  const bool running = game_process != nullptr;
  d->AddText(title_font, 78, {o.x + 44, o.y + 24}, kWhite, "WWE 2K15");
  d->AddText(ui_font, 20, {o.x + 48, o.y + 122}, kMuted, tr("Version PC non officielle · rendu Vulkan · manette SDL"));
  Chip(d, {o.x + 48, o.y + 176}, tr("FICHIERS DU JEU"), has ? tr("TROUVÉS") : tr("À SÉLECTIONNER"), has ? IM_COL32(110, 200, 80, 255) : kGold);
  Chip(d, {o.x + 330, o.y + 176}, tr("RENDU"), "VULKAN", kRed);
  { const char* pn = pad ? SDL_GetGamepadName(pad) : nullptr; Chip(d, {o.x + 500, o.y + 176}, tr("MANETTE"), pn ? pn : tr("AUCUNE DÉTECTÉE"), pn ? IM_COL32(110, 200, 80, 255) : kMuted); }
  {
    const bool tu = HasTitleUpdate();
    const int dlc = DlcCount();
    const std::string dlc_text = dlc ? std::to_string(dlc) + " PACK(S)" : tr("AUCUN");
    Chip(d, {o.x + 48, o.y + 226}, "TITLE UPDATE", tu ? tr("TU4 INSTALLÉE") : tr("MANQUANTE"), tu ? IM_COL32(110, 200, 80, 255) : kGold);
    Chip(d, {o.x + 330, o.y + 226}, "DLC", dlc_text.c_str(), dlc ? IM_COL32(110, 200, 80, 255) : kMuted);
  }

  ImGui::SetCursorScreenPos({o.x + 48, o.y + 292});
  if (Tile("play", running ? tr("EN COURS…") : (has ? tr("JOUER") : tr("CHOISIR LE JEU")), {400, 96}, false, true, 40) && !running) {
    if (has) LaunchGame(); else page = "RÉGLAGES";
  }
  if (!running && has && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))) LaunchGame();
  ImGui::SetCursorScreenPos({o.x + 480, o.y + 314});
  if (Tile("logs", tr("JOURNAUX"), {190, 52}, false, false, 20)) OpenFolder(root / L"logs");

  if (!status.empty()) d->AddText(ui_font, 19, {o.x + 48, o.y + 416}, kWhite, status.c_str());
  d->AddText(ui_font, 15, {o.x + 48, o.y + area.y - 30}, kMuted,
             tr("Aucune donnée du jeu n'est incluse. Une copie légale du jeu est requise."));
}

bool Choice(const char* id, ImVec2 pos, const char* const* labels, const int* values, int count,
            int& value, float width = 110);

void DrawSettings(ImDrawList* d, ImVec2 o, ImVec2 area) {
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("RÉGLAGES"), tr("Dossier du jeu et options de lancement"));
  d->AddText(ui_font, 16, {o.x + 48, o.y + 112}, kMuted, tr("DOSSIER DU JEU XBOX 360 (doit contenir default.xex)"));
  d->AddRectFilled({o.x + 48, o.y + 138}, {o.x + area.x - 48, o.y + 184}, IM_COL32(10, 10, 12, 255));
  const std::string path = game_data.empty() ? std::string(tr("Aucun dossier sélectionné")) : game_data.string();
  d->AddText(ui_font, 18, {o.x + 60, o.y + 150}, game_data.empty() ? kMuted : kWhite, path.c_str());
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 204});
  if (Tile("browse", tr("PARCOURIR…"), {210, 48}, false, true, 20)) BrowseGame();
  ImGui::SetCursorScreenPos({o.x + 280, o.y + 204});
  if (Tile("save", tr("ENREGISTRER"), {210, 48}, false, false, 20)) { SaveSettings(); status = tr("Réglages enregistrés."); }
  ImGui::SetCursorScreenPos({o.x + 512, o.y + 204});
  if (Tile("packages", tr("AJOUTER TU / DLC…"), {260, 48}, false, false, 20)) AddPackages();

  d->AddText(ui_font, 18, {o.x + 48, o.y + 280}, kWhite, tr("LANGUE"));
  d->AddText(ui_font, 15, {o.x + 48, o.y + 304}, kMuted, tr("Langue du jeu et de ce lanceur."));
  if (Choice("lang", {o.x + 48, o.y + 330}, i18n::kLanguageNames, i18n::kLanguageIds,
             i18n::kLanguageCount, opt.language, 150))
    SaveOptions();

  ImGui::SetCursorScreenPos({o.x + 48, o.y + 400});
  if (ImGui::Checkbox(tr("Désactiver le hook Vulkan d'OBS"), &disable_obs_vulkan_hook)) SaveSettings();
  d->AddText(ui_font, 15, {o.x + 76, o.y + 432}, kMuted, disable_obs_vulkan_hook
      ? tr("Activé : évite l'injection d'OBS dans le rendu Vulkan du jeu.")
      : tr("Désactivé : OBS peut capturer le jeu via son hook Vulkan."));
  d->AddText(ui_font, 15, {o.x + 48, o.y + 476}, kMuted,
             tr("Rendu : Vulkan (GPU haute performance)  ·  Graphismes et clavier : onglets GRAPHISMES et CONTRÔLES"));
  d->AddText(ui_font, 15, {o.x + 48, o.y + 502}, kMuted,
             tr("AJOUTER TU / DLC : la Title Update 4 (tu00000004_00000000) est requise ; les DLC (545408B6\\00000002) sont optionnels."));
  if (!status.empty()) d->AddText(ui_font, 19, {o.x + 48, o.y + 536}, kWhite, status.c_str());
}

// One row of mutually exclusive choices drawn as tiles; returns true when the value changed.
bool Choice(const char* id, ImVec2 pos, const char* const* labels, const int* values, int count,
            int& value, float width) {
  bool changed = false;
  for (int i = 0; i < count; ++i) {
    ImGui::SetCursorScreenPos({pos.x + i * (width + 12), pos.y});
    ImGui::PushID(id);
    ImGui::PushID(i);
    if (Tile(labels[i], labels[i], {width, 44}, value == values[i], false, 18) && value != values[i]) {
      value = values[i];
      changed = true;
    }
    ImGui::PopID();
    ImGui::PopID();
  }
  return changed;
}

// Graphics presets: 1 performance, 2 balanced, 3 quality; 0 = custom.
int CurrentPreset() {
  if (opt.resolution_scale == 1 && opt.anisotropic == 0 && opt.aa == 0) return 1;
  if (opt.resolution_scale == 1 && opt.anisotropic == 3 && opt.aa == 0) return 2;
  if (opt.resolution_scale == 2 && opt.anisotropic == 5 && opt.aa == 1) return 3;
  return 0;
}

void ApplyPreset(int preset) {
  switch (preset) {
    case 1: opt.resolution_scale = 1; opt.anisotropic = 0; opt.aa = 0; break;
    case 2: opt.resolution_scale = 1; opt.anisotropic = 3; opt.aa = 0; break;
    case 3: opt.resolution_scale = 2; opt.anisotropic = 5; opt.aa = 1; break;
  }
}

void DrawGraphics(ImDrawList* d, ImVec2 o, ImVec2 area) {
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("GRAPHISMES"), tr("Appliqués au prochain lancement du jeu"));
  const float col2 = o.x + area.x * 0.5f + 16;
  auto label = [&](float x, float y, const char* t, const char* help) {
    d->AddText(ui_font, 18, {x, y}, kWhite, t);
    if (help) d->AddText(ui_font, 14, {x, y + 23}, kMuted, help);
  };
  // Left column: quality.
  float y = o.y + 104;
  label(o.x + 48, y, tr("PRÉRÉGLAGE"), tr("Performance pour les petites configurations, Qualité pour les PC puissants."));
  {
    const char* l[] = {tr("PERFORMANCE"), tr("ÉQUILIBRÉ"), tr("QUALITÉ")};
    static const int v[] = {1, 2, 3};
    int preset = CurrentPreset();
    if (Choice("preset", {o.x + 48, y + 46}, l, v, 3, preset, 170)) { ApplyPreset(preset); SaveOptions(); }
  }
  y += 106;
  label(o.x + 48, y, tr("RÉSOLUTION DE RENDU"), tr("1x = 720p comme sur Xbox 360. 2x et plus : plus net, bien plus lourd (expérimental)."));
  {
    static const char* l[] = {"1x", "2x", "3x", "4x"};
    static const int v[] = {1, 2, 3, 4};
    if (Choice("res", {o.x + 48, y + 46}, l, v, 4, opt.resolution_scale, 110)) SaveOptions();
  }
  y += 106;
  label(o.x + 48, y, tr("FILTRAGE DES TEXTURES"), tr("Rend le sol et le public plus nets de loin."));
  {
    const char* l[] = {tr("AUCUN"), "2x", "4x", "8x", "16x"};
    static const int v[] = {0, 2, 3, 4, 5};
    if (Choice("aniso", {o.x + 48, y + 46}, l, v, 5, opt.anisotropic, 98)) SaveOptions();
  }
  y += 106;
  label(o.x + 48, y, tr("ANTICRÉNELAGE"), tr("Adoucit les bords en escalier (FXAA)."));
  {
    const char* l[] = {tr("AUCUN"), "FXAA", "FXAA +"};
    static const int v[] = {0, 1, 2};
    if (Choice("aa", {o.x + 48, y + 46}, l, v, 3, opt.aa, 150)) SaveOptions();
  }
  // Right column: display.
  y = o.y + 104;
  label(col2, y, tr("AFFICHAGE"), nullptr);
  {
    const char* l[] = {tr("PLEIN ÉCRAN"), tr("FENÊTRE")};
    static const int v[] = {1, 0};
    int fs = opt.fullscreen ? 1 : 0;
    if (Choice("mode", {col2, y + 30}, l, v, 2, fs, 200)) { opt.fullscreen = fs != 0; SaveOptions(); }
  }
  y += 90;
  label(col2, y, tr("TAILLE DE LA FENÊTRE"), nullptr);
  {
    const char* l[] = {"720p", "900p", "1080p", "1440p", "4K"};
    static const int v[] = {0, 1, 2, 3, 4};
    if (Choice("wsize", {col2, y + 30}, l, v, 5, opt.window_size, 96)) SaveOptions();
  }
  y += 90;
  label(col2, y, tr("ÉCRAN"), nullptr);
  {
    const char* l[] = {tr("AUTO"), "1", "2", "3"};
    static const int v[] = {0, 1, 2, 3};
    if (Choice("monitor", {col2, y + 30}, l, v, 4, opt.monitor, 96)) SaveOptions();
  }
  y += 94;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Bandes noires (garder le format 16:9)"), &opt.letterbox)) SaveOptions();
  y += 34;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Afficher le compteur d'images/s"), &opt.show_fps)) SaveOptions();
  y += 34;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Rendu précis (FSI)"), &opt.fsi)) SaveOptions();
  d->AddText(ui_font, 14, {col2 + 28, y + 27}, kMuted, tr("Corrige les écrans noirs des entrées (NVIDIA, Intel)."));
  y += 54;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Réduire l'utilisation du processeur (expérimental)"), &opt.cpu_yield)) SaveOptions();
}

void DrawControls(ImDrawList* d, ImVec2 o, ImVec2 area) {
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("CONTRÔLES"), tr("Manette ou clavier"));
  d->AddText(ui_font, 18, {o.x + 48, o.y + 110}, kWhite,
             tr("Branche ta manette avant de lancer le jeu. SDL gère les manettes Xbox et PlayStation, dont la DualSense."));
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 160});
  if (ImGui::Checkbox(tr("Jouer au clavier (manette virtuelle)"), &opt.keyboard)) SaveOptions();
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 196});
  if (ImGui::Checkbox(tr("Clavier AZERTY (français)"), &opt.azerty)) SaveOptions();
  d->AddText(ui_font, 16, {o.x + 48, o.y + 246}, opt.keyboard ? kWhite : kMuted,
             tr(opt.azerty ? i18n::kKeysAzerty : i18n::kKeysQwerty));
}

void DrawTextPage(ImDrawList* d, ImVec2 o, ImVec2 area, const char* title, const char* sub, const char* body) {
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, title, sub);
  d->AddText(ui_font, 20, {o.x + 48, o.y + 120}, kWhite, body, nullptr, area.x - 96);
}

void DrawUi() {
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  ImGui::PushFont(ui_font, 20.0f);
  ImGui::Begin("WWE 2K15 Launcher", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  ImDrawList* d = ImGui::GetWindowDrawList();
  const ImVec2 size = io.DisplaySize;

  int idx = TabIndex();
  if (ImGui::IsKeyPressed(ImGuiKey_Q) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) idx = (idx + kTabCount - 1) % kTabCount;
  if (ImGui::IsKeyPressed(ImGuiKey_E) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) idx = (idx + 1) % kTabCount;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) idx = 0;
  page = kTabs[idx];

  DrawBackground(d, size, (float)ImGui::GetTime());
  DrawHeader(d);
  const ImVec2 o{44, 100};
  const ImVec2 area{size.x - 88, size.y - 100 - 64};
  if (page == "JOUER") DrawPlay(d, o, area);
  else if (page == "RÉGLAGES") DrawSettings(d, o, area);
  else if (page == "GRAPHISMES") DrawGraphics(d, o, area);
  else if (page == "CONTRÔLES") DrawControls(d, o, area);
  else if (page == "MODS") {
    DrawTextPage(d, o, area, tr("MODS"), tr("Préparation"),
        tr("Espace de préparation des mods.\nLe remplacement de fichiers n'est pas encore activé dans cette beta."));
    ImGui::SetCursorScreenPos({o.x + 48, o.y + 230});
    if (Tile("mods", tr("OUVRIR LE DOSSIER MODS"), {320, 48}, false, true, 19)) OpenFolder(root / L"mods");
  } else if (page == "SAUVEGARDES") {
    DrawTextPage(d, o, area, tr("SAUVEGARDES"), tr("Données utilisateur"),
        tr("Sauvegardes et données utilisateur stockées dans\nLocalAppData\\WWE2K15."));
    ImGui::SetCursorScreenPos({o.x + 48, o.y + 230});
    if (Tile("saves", tr("OUVRIR LE DOSSIER"), {280, 48}, false, true, 19)) OpenFolder(user_data);
  } else {
    DrawTextPage(d, o, area, tr("À PROPOS"), "WWE 2K15 PC",
        tr("Portage PC non officiel de WWE 2K15, basé sur ReXGlue.\nAucune donnée du jeu n'est fournie.\n\nSans affiliation avec WWE, 2K ou Yuke's."));
    ImGui::SetCursorScreenPos({o.x + 48, o.y + 250});
    if (Tile("gh", tr("DÉPÔT GITHUB"), {250, 48}, false, true, 19))
      ShellExecuteW(nullptr, L"open", L"https://github.com/iMrBurn/wwe2k15-pc", nullptr, nullptr, SW_SHOWNORMAL);
  }
  DrawFooter(d, size);
  ImGui::End();
  ImGui::PopFont();
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp)) return 1;
  if (msg == WM_SIZE && wp != SIZE_MINIMIZED) { resize_w = LOWORD(lp); resize_h = HIWORD(lp); return 0; }
  if (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_KEYMENU) return 0;
  if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  root = ExeDir(); LoadSettings();
  HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, instance, icon, nullptr, nullptr, nullptr,
                 L"WWE2K15Launcher", icon};
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"WWE 2K15 PC", WS_OVERLAPPEDWINDOW,
      100, 100, 1280, 760, nullptr, nullptr, wc.hInstance, nullptr);
  { BOOL dark = TRUE; DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)); }
  if (!MakeDevice(hwnd)) { MessageBoxW(hwnd,L"Impossible d'initialiser l'interface graphique.",L"WWE 2K15 PC",MB_ICONERROR); return 1; }
  ShowWindow(hwnd,SW_SHOWDEFAULT); UpdateWindow(hwnd);
  IMGUI_CHECKVERSION(); ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle(); style.WindowRounding=0; style.FrameRounding=0; style.ChildRounding=0;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(.03f,.03f,.035f,1);
  style.Colors[ImGuiCol_Text] = ImVec4(.95f,.95f,.96f,1);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(.78f,.11f,.18f,1);
  ImGuiIO& io=ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  io.IniFilename = nullptr;
  ui_font = title_font = io.Fonts->AddFontDefault();
  if (fs::exists(L"C:/Windows/Fonts/bahnschrift.ttf")) ui_font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/bahnschrift.ttf", 20.0f);
  else if (fs::exists(L"C:/Windows/Fonts/segoeui.ttf")) ui_font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 20.0f);
  title_font = ui_font;
  if (fs::exists(L"C:/Windows/Fonts/impact.ttf")) title_font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/impact.ttf", 40.0f);
  logo_font = title_font;
  io.FontDefault = ui_font;
  InitPad();
  ImGui_ImplWin32_Init(hwnd); ImGui_ImplDX11_Init(device.Get(),context.Get());
  bool done=false;
  while (!done) {
    MSG msg; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if(msg.message==WM_QUIT) done=true; }
    if(done) break;
    // Idle cheaply: nothing to draw while minimised, and only a slow refresh while the game runs.
    if (IsIconic(hwnd)) { Sleep(150); continue; }
    if (game_process) Sleep(60);
    if(resize_w && resize_h) { target.Reset(); swapchain->ResizeBuffers(0,resize_w,resize_h,DXGI_FORMAT_UNKNOWN,0); resize_w=resize_h=0; MakeTarget(); }
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); PollPad(); ImGui::NewFrame(); DrawUi(); ImGui::Render();
    const float bg[]={.035f,.035f,.04f,1}; context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr); context->ClearRenderTargetView(target.Get(),bg);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); swapchain->Present(1,0);
  }
  ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
  target.Reset(); swapchain.Reset(); context.Reset(); device.Reset(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName,wc.hInstance);
  if (game_process) CloseHandle(game_process);
  if (pad) SDL_CloseGamepad(pad);
  SDL_Quit();
  CoUninitialize();
  return 0;
}
