#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
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
#include "saves.h"
#include "stfs.h"
#include "updater.h"

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

// The game is a folder holding default.xex, or an Xbox 360 disc image (.iso) read in place.
bool IsIso(const fs::path& p) {
  std::error_code ec;
  std::wstring ext = p.extension().wstring();
  for (auto& c : ext) c = towlower(c);
  return ext == L".iso" && fs::is_regular_file(p, ec);
}
bool ValidGame(const fs::path& p) {
  std::error_code ec;
  return !p.empty() && (IsIso(p) || fs::exists(p / L"default.xex", ec));
}
// Where the title update and DLC go: the game folder, or game_update\ beside the launcher for a
// disc image (the game looks there too).
fs::path Extras() { return IsIso(game_data) ? root / L"game_update" : game_data; }
std::string page = "JOUER", status;
HANDLE game_process = nullptr;
Updater updater;
fs::file_time_type launch_time{};
std::chrono::steady_clock::time_point launch_start{};
std::string start_failure;  // plain-language reason the game stopped right after starting
bool disable_obs_vulkan_hook = true;

// Game options passed to wwe2k15.exe as cvars (saved in launcher-options.ini).
struct Options {
  bool fullscreen = true;
  int resolution = 720;       // 720, 1080, 1440 or 2160: render scale and windowed size
  int monitor = 0;            // monitor: 0 = default, 1..3
  int aa = 0;                 // swap_post_effect: 0 none, 1 fxaa, 2 fxaa_extreme
  bool letterbox = true;      // present_letterbox
  bool auto_backup = true;    // back up saves before each launch (keeps 10)
  bool mods = true;           // wwe2k15_mods
  int anisotropic = 3;        // anisotropic_override: 0 off, 2 = 2x, 3 = 4x, 4 = 8x, 5 = 16x
  bool fsi = true;            // render_target_path_vulkan = auto (FSI except on AMD, where it is very slow)
  bool keyboard = false;      // mnk_mode
  bool azerty = true;         // French keyboard layout for the keyboard pad
  int language = 0;           // user_language (XLanguage): 1 en, 3 de, 4 fr, 5 es, 6 it; 0 = unset
  bool cpu_yield = true;      // wwe2k15_render_wait_yield
  bool vsync = true;          // no tearing (vulkan_allow_present_mode_immediate = false)
  bool show_fps = false;      // wwe2k15_show_fps
  bool native = false;        // experimental native renderer: wwe2k15_native.exe
  int native_resolution = 0;  // native renderer: svr_render_scale, 0 = automatic (GPU and screen)
  std::string gpu;            // native renderer only: Vulkan name of the GPU ("" = automatic)
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
  {
    // The game started without the launcher keeps saves in Documents\wwe2k15: adopt them once.
    wchar_t docs[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs);
    const fs::path old_root = fs::path(docs) / L"wwe2k15";
    std::error_code mec;
    if (saves::ProfileFolders(user_data).empty())
      for (const auto& p : saves::ProfileFolders(old_root))
        fs::copy(p, user_data / p.filename(), fs::copy_options::recursive, mec);
  }
  const fs::path cfg = root / L"launcher-settings.txt";
  std::error_code ec;
  if (fs::exists(cfg, ec)) {
    std::wstring line;
    std::wifstream in(cfg);
    std::getline(in, line);
    if (!line.empty() && ValidGame(line)) game_data = line;
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
  // A disc image put in game\ or beside the launcher.
  for (const auto& dir : {root / L"game", root})
    for (const auto& e : fs::directory_iterator(dir, ec))
      if (game_data.empty() && IsIso(e.path())) game_data = e.path();
  LoadOptions();
}

bool HasGame() { return ValidGame(game_data); }

// The game is generated from Title Update 4: default.xexp must sit beside default.xex (or in
// game_update\ for a disc image).
bool NonEmptyFile(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec) && fs::file_size(p, ec) > 0;
}

bool HasTitleUpdate() { return HasGame() && NonEmptyFile(Extras() / L"default.xexp"); }

// A problem with the game files that stops the game from starting, in words ("" = none seen).
std::string GameFilesProblem() {
  if (!HasGame()) return {};
  if (!IsIso(game_data) && !NonEmptyFile(game_data / L"default.xex"))
    return tr("default.xex est vide ou abîmé : les fichiers du jeu sont incomplets. Copie-les de nouveau.");
  if (!HasTitleUpdate())
    return tr("La Title Update 4 (default.xexp) est absente ou vide. Ajoute-la : RÉGLAGES > AJOUTER TU / DLC.");
  return {};
}

// Why the game stopped at start, read from its newest log ("" = nothing recognised).
std::string StartFailureFromLog(fs::file_time_type since) {
  fs::path best;
  fs::file_time_type best_time{};
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(root / L"logs", ec)) {
    const std::wstring n = e.path().filename().wstring();
    if (e.path().extension() != L".log" || n.rfind(L"wwe2k15_", 0) != 0) continue;
    const auto t = e.last_write_time(ec);
    if (t >= since && t > best_time) { best = e.path(); best_time = t; }
  }
  if (best.empty()) return {};
  std::ifstream in(best);
  std::string line, found, tu_version;
  bool no_function = false;
  while (std::getline(in, line)) {
    // "XEX patch applied successfully: base version: 0.0.0.4, new version: 0.0.6.4" (TU4 gives 0.0.6.4)
    if (const size_t v = line.find("new version: "); v != std::string::npos) {
      tu_version.clear();
      for (size_t i = v + 13; i < line.size() && (isdigit((unsigned char)line[i]) || line[i] == '.'); ++i) tu_version += line[i];
    }
    if (line.find("No function registered at") != std::string::npos) no_function = true;
    if (line.find("Failed to load XEX") != std::string::npos)
      found = tr("Le jeu n'a pas pu charger default.xex. Vérifie que la Title Update 4 est bien installée et qu'elle correspond à ton jeu (même région et même édition, default.xex non modifié), et que les fichiers du jeu sont complets. Détails : bouton JOURNAUX.");
    else if (found.empty() && (line.find("no default.xexp") != std::string::npos ||
                               line.find("Could not find default.xex") != std::string::npos))
      found = tr("La Title Update 4 est introuvable. Ajoute-la : RÉGLAGES > AJOUTER TU / DLC.");
    else if (found.empty() && line.find("XEX patch") != std::string::npos &&
             (line.find("fail") != std::string::npos || line.find("error") != std::string::npos))
      found = tr("La Title Update 4 ne correspond pas à ton jeu (autre région ou édition, ou default.xex déjà modifié). Utilise le jeu d'origine et la TU4 de la même région. Détails : bouton JOURNAUX.");
  }
  // A Title Update that is not TU4: the game starts, then calls code that only TU4 has.
  if (found.empty() && tu_version != "0.0.6.4" && (no_function || !tu_version.empty()))
    found = tr("Mauvaise Title Update : la version ") + (tu_version.empty() ? std::string(tr("inconnue")) : tu_version) +
            tr(" est installée, il faut la Title Update 4 (0.0.6.4). Supprime game_update\\default.xexp (ou le dossier update) et ajoute le bon paquet (RÉGLAGES > AJOUTER TU / DLC).");
  return found;
}

int DlcCount() {
  std::error_code ec;
  int count = 0;
  if (HasGame())
    for (const auto& e : fs::directory_iterator(Extras() / L"dlc", ec))
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
    fs::create_directories(Extras() / L"dlc", ec);
    fs::copy_file(file, Extras() / L"dlc" / file.filename(), fs::copy_options::overwrite_existing, ec);
    msg = ec ? name + tr(" : copie impossible (") + ec.message() + ")."
             : tr("DLC ajouté : ") + pkg.display_name + ".";
    return !ec;
  }
  if (pkg.content_type != stfs::kTypeTitleUpdate) { msg = name + tr(" : type de package non pris en charge."); return false; }
  if (pkg.version != 4) { msg = name + tr(" : seule la Title Update 4 est prise en charge."); return false; }
  for (size_t i = 0; i < pkg.entries.size(); ++i) {
    const auto& entry = pkg.entries[i];
    const std::string rel = pkg.PathOf(i);
    const fs::path dst = rel == "default.xexp" ? Extras() / L"default.xexp"
                                               : Extras() / L"update" / fs::path(rel);
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
  out << "fullscreen=" << opt.fullscreen << "\nresolution=" << opt.resolution
      << "\nanisotropic=" << opt.anisotropic << "\nfsi=" << opt.fsi
      << "\nnative=" << opt.native << "\nnative_resolution=" << opt.native_resolution << "\ngpu=" << opt.gpu
      << "\nlanguage=" << opt.language << "\nkeyboard=" << opt.keyboard << "\nazerty=" << opt.azerty << "\ngpu_wait_yield=" << opt.cpu_yield << "\nshow_fps=" << opt.show_fps << "\nvsync=" << opt.vsync << "\nmonitor=" << opt.monitor << "\naa=" << opt.aa << "\nletterbox=" << opt.letterbox << "\nauto_backup=" << opt.auto_backup << "\nmods=" << opt.mods << "\n";
}

void LoadOptions() {
  std::ifstream in(root / L"launcher-options.ini");
  std::string line;
  while (std::getline(in, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq);
    const int v = std::atoi(line.c_str() + eq + 1);
    if (k == "gpu") {
      opt.gpu = line.substr(eq + 1);
      while (!opt.gpu.empty() && (opt.gpu.back() == '\r' || opt.gpu.back() == '\n')) opt.gpu.pop_back();
      continue;
    }
    if (k == "fullscreen") opt.fullscreen = v != 0;
    else if (k == "resolution") opt.resolution = v == 540 || v == 1080 || v == 1440 || v == 2160 ? v : 720;
    // Files from before the single resolution choice.
    else if (k == "resolution_scale") opt.resolution = v >= 3 ? 2160 : v == 2 ? 1440 : 720;
    else if (k == "monitor") opt.monitor = std::clamp(v, 0, 3);
    else if (k == "aa") opt.aa = std::clamp(v, 0, 2);
    else if (k == "letterbox") opt.letterbox = v != 0;
    else if (k == "auto_backup") opt.auto_backup = v != 0;
    else if (k == "mods") opt.mods = v != 0;
    else if (k == "anisotropic") opt.anisotropic = std::clamp(v, 0, 5);
    else if (k == "fsi") opt.fsi = v != 0;
    else if (k == "keyboard") opt.keyboard = v != 0;
    else if (k == "azerty") opt.azerty = v != 0;
    else if (k == "language") opt.language = v;
    else if (k == "gpu_wait_yield") opt.cpu_yield = v != 0;  // (the old cpu_yield key defaulted to off)
    else if (k == "show_fps") opt.show_fps = v != 0;
    else if (k == "vsync") opt.vsync = v != 0;
    else if (k == "native") opt.native = v != 0;
    else if (k == "native_resolution")
      opt.native_resolution = v == 540 || v == 720 || v == 1080 || v == 1440 || v == 2160 ? v : 0;
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

// The GPU emulation renders whole multiples of the Xbox 360's 720p: 1080p renders at 2x (1440p)
// and is scaled down to the 1080p window or screen, which is sharper than a native 1080p.
int RenderScale(int resolution) { return resolution <= 720 ? 1 : resolution >= 2160 ? 3 : resolution >= 1080 ? 2 : 1; }

bool NativeAvailable() {
  std::error_code ec;
  return fs::exists(root / L"wwe2k15_native.exe", ec) && fs::exists(root / L"dxcompiler.dll", ec);
}
bool UseNative() { return opt.native && NativeAvailable(); }

std::wstring OptionArgs() {
  auto b = [](bool v) { return std::wstring(v ? L"true" : L"false"); };
  if (UseNative()) {
    // The native renderer has no Xenos GPU emulation: only the options it shares or its own.
    return L" --fullscreen=" + b(opt.fullscreen) +
           L" --svr_render_scale=" + std::to_wstring(opt.native_resolution) +
           L" --svr_fxaa=" + std::to_wstring(opt.aa) +
           L" --bd_anisotropy=" + std::to_wstring(opt.anisotropic < 2 ? 0 : 1 << (opt.anisotropic - 1)) +
           (opt.gpu.empty() ? std::wstring() : L" --svr_gpu_device=\"" + Widen(opt.gpu.c_str()) + L"\"") +
           L" --mnk_mode=" + b(opt.keyboard) +
           L" --user_language=" + std::to_wstring(opt.language) +
           L" --wwe2k15_render_wait_yield=" + b(opt.cpu_yield) +
           L" --wwe2k15_show_fps=" + b(opt.show_fps) +
           L" --window_width=" + std::to_wstring((opt.native_resolution ? std::max(opt.native_resolution, 720) : 1080) * 16 / 9) +
           L" --window_height=" + std::to_wstring(opt.native_resolution ? std::max(opt.native_resolution, 720) : 1080) +
           L" --monitor=" + std::to_wstring(opt.monitor) +
           L" --wwe2k15_mods=" + b(opt.mods) +
           (opt.keyboard && opt.azerty
                ? std::wstring(L" --keybind_lstick_up=Z --keybind_lstick_left=Q --keybind_lstick_down=S"
                               L" --keybind_lstick_right=D --keybind_left_trigger=A,I"
                               L" --keybind_back=W,Tab")
                : std::wstring());
  }
  return L" --fullscreen=" + b(opt.fullscreen) +
         L" --resolution_scale=" + std::to_wstring(RenderScale(opt.resolution)) +
         L" --anisotropic_override=" + std::to_wstring(opt.anisotropic) +
         L" --render_target_path_vulkan=" + (opt.fsi ? L"auto" : L"rtv") +
         L" --mnk_mode=" + b(opt.keyboard) +
         L" --user_language=" + std::to_wstring(opt.language) +
         L" --wwe2k15_render_wait_yield=" + b(opt.cpu_yield) +
         L" --wwe2k15_show_fps=" + b(opt.show_fps) +
         L" --vulkan_allow_present_mode_immediate=" + b(!opt.vsync) +
         L" --window_width=" + std::to_wstring(std::max(opt.resolution, 720) * 16 / 9) +
         L" --window_height=" + std::to_wstring(std::max(opt.resolution, 720)) +
         L" --monitor=" + std::to_wstring(opt.monitor) +
         L" --swap_post_effect=" + (opt.aa == 2 ? L"fxaa_extreme" : opt.aa == 1 ? L"fxaa" : L"none") +
         L" --present_letterbox=" + b(opt.letterbox) +
         L" --wwe2k15_mods=" + b(opt.mods) +
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

// One file picker for both kinds of game: default.xex (its folder becomes the game folder) or an
// .iso (the file itself).
void BrowseGame() {
  IFileDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_FILEMUSTEXIST);
  const std::wstring game_filter = Widen(tr("Jeu Xbox 360 (default.xex, *.iso)"));
  const std::wstring all_filter = Widen(tr("Tous les fichiers"));
  const COMDLG_FILTERSPEC filter[] = {{game_filter.c_str(), L"default.xex;*.iso"},
                                      {all_filter.c_str(), L"*.*"}};
  dialog->SetFileTypes(2, filter);
  dialog->SetTitle(Widen(tr("Choisis default.xex ou l'image disque (.iso) de WWE 2K15")).c_str());
  if (SUCCEEDED(dialog->Show(nullptr))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        const fs::path selected(path);
        std::wstring name = selected.filename().wstring();
        for (auto& c : name) c = towlower(c);
        if (IsIso(selected)) {
          game_data = selected;
          SaveSettings();
          status = tr("Image disque enregistrée.");
        } else if (name == L"default.xex") {
          game_data = selected.parent_path();
          SaveSettings();
          status = tr("Dossier du jeu enregistré.");
        } else status = tr("Ce fichier n'est ni default.xex ni une image .iso.");
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
}

SDL_Gamepad* pad = nullptr;
bool keyboard_forced = false;  // this launch plays on the keyboard because no gamepad was found

void LaunchGame() {
  if (game_process && WaitForSingleObject(game_process, 0) == WAIT_TIMEOUT) {
    status = tr("Le jeu tourne déjà.");
    return;
  }
  if (game_process) { CloseHandle(game_process); game_process = nullptr; }
  if (updater.Busy()) { status = tr("Attends la fin de la mise à jour."); return; }
  const bool native = UseNative();
  const fs::path exe = root / (native ? L"wwe2k15_native.exe" : L"wwe2k15.exe");
  if (!fs::exists(exe)) { status = tr("wwe2k15.exe est introuvable : reconstruis le projet."); return; }
  if (!HasGame()) { status = tr("Sélectionne le dossier qui contient default.xex."); page = "RÉGLAGES"; return; }
  if (!HasTitleUpdate()) { status = tr("Ajoute la Title Update 4 (RÉGLAGES > AJOUTER TU / DLC)."); page = "RÉGLAGES"; return; }
  SaveSettings();
  launch_time = fs::file_time_type::clock::now();
  launch_start = std::chrono::steady_clock::now();
  start_failure.clear();
  if (opt.auto_backup) {
    std::string err;
    if (saves::CreateBackup(user_data, err)) saves::PruneBackups(user_data, 10);
  }
  // The game does not wait for a pad: with none and no keyboard, the title screen could not be answered.
  // Play on the keyboard for this launch only; the saved option stays as it is.
  const bool saved_keyboard = opt.keyboard;
  keyboard_forced = !pad && !opt.keyboard;
  if (keyboard_forced) opt.keyboard = true;
  const std::wstring args = L"--game_data_root=\"" + game_data.wstring() +
      L"\" --user_data_root=\"" + user_data.wstring() +
      L"\"" + (native ? L"" : L" --gpu_plugin=xenos --gpu_backend=vulkan") +
      L" --input_backend=sdl --hid_mappings_file=\"" +
      (root / L"gamecontrollerdb.txt").wstring() + L"\"" + OptionArgs();
  opt.keyboard = saved_keyboard;
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
  char code_hex[16];
  snprintf(code_hex, sizeof(code_hex), "0x%08lX", static_cast<unsigned long>(exit_code));
  status = tr("Le jeu s'est arrêté (code ") + (exit_code < 256 ? std::to_string(exit_code) : std::string(code_hex)) +
           tr("). Consulte JOURNAUX.");
  // A failure within the first seconds is the game files, not a crash during play.
  if (exit_code != 0 && std::chrono::steady_clock::now() - launch_start < std::chrono::seconds(30))
    start_failure = StartFailureFromLog(launch_time - std::chrono::seconds(2));
}

// SDL3 gamepad input (DualSense, Xbox, ...) fed into ImGui navigation; ImGui's own backend is XInput-only.
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
  const std::string ver_text = std::string("v" WWE2K15_VERSION);
  const char* ver = ver_text.c_str();
  d->AddText(ui_font, 15, {size.x - TextW(ui_font, 15, ver) - 44, size.y - 32}, kMuted, ver);
}

fs::path NewestReport();

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

  // No gamepad and no keyboard play: the game would start but could not be played.
  if (!pad && !running) {
    const float cx = o.x + 800, cw = area.x - 800 - 48;
    float cy = o.y + 170;
    if (!opt.keyboard) {
      const char* msg = tr("Aucune manette détectée : branche-en une, ou joue au clavier.");
      d->AddText(ui_font, 16, {cx, cy}, kGold, msg, nullptr, cw);
      cy += ui_font->CalcTextSizeA(16, FLT_MAX, cw, msg).y + 8;
      ImGui::SetCursorScreenPos({cx, cy});
      if (Tile("kb_on", tr("ACTIVER LE CLAVIER"), {200, 36}, false, false, 16)) { opt.keyboard = true; SaveOptions(); }
      ImGui::SetCursorScreenPos({cx + 208, cy});
      if (Tile("kb_page", tr("CONTRÔLES"), {120, 36}, false, false, 16)) page = "CONTRÔLES";
      cy += 44;
    }
    d->AddText(ui_font, 15, {cx, cy}, kMuted,
               tr(opt.azerty ? "Clavier AZERTY : Z Q S D, Espace = A, Entrée = Start (CONTRÔLES pour changer)."
                             : "Clavier QWERTY : W A S D, Espace = A, Entrée = Start (CONTRÔLES pour changer)."),
               nullptr, cw);
  }

  ImGui::SetCursorScreenPos({o.x + 48, o.y + 292});
  if (Tile("play", running ? tr("EN COURS…") : (has ? tr("JOUER") : tr("CHOISIR LE JEU")), {400, 96}, false, true, 40) && !running) {
    if (has) LaunchGame(); else page = "RÉGLAGES";
  }
  if (!running && has && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))) LaunchGame();
  ImGui::SetCursorScreenPos({o.x + 480, o.y + 314});
  if (Tile("logs", tr("JOURNAUX"), {190, 52}, false, false, 20)) OpenFolder(root / L"logs");

  if (!status.empty()) d->AddText(ui_font, 19, {o.x + 48, o.y + 416}, kWhite, status.c_str());
  if (running && keyboard_forced)
    d->AddText(ui_font, 17, {o.x + 48, o.y + 446}, kGold,
               tr("Aucune manette détectée : le clavier a été activé pour cette partie (Espace = A, Entrée = Start)."));
  // Red note: the game files look wrong, or the game just failed to start because of them.
  float extra = 0;
  {
    const std::string note = !running && !start_failure.empty() ? start_failure : (running ? std::string() : GameFilesProblem());
    if (!note.empty()) {
      const float wrap = area.x - 96;
      const ImU32 red = IM_COL32(255, 96, 96, 255);
      d->AddText(ui_font, 17, {o.x + 48, o.y + 446}, red, note.c_str(), nullptr, wrap);
      extra = ui_font->CalcTextSizeA(17, FLT_MAX, wrap, note.c_str()).y + 8;
    }
  }
  if (const std::string latest = updater.AvailableVersion(); !latest.empty()) {
    const std::string notice = tr("Mise à jour disponible : ") + latest;
    d->AddText(ui_font, 17, {o.x + 760, o.y + 318}, kGold, notice.c_str());
    ImGui::SetCursorScreenPos({o.x + 760, o.y + 346});
    if (Tile("update_go", tr("VOIR LA MISE À JOUR"), {260, 40}, false, false, 17)) page = "À PROPOS";
  }
  if (!running) {
    static fs::path report;
    static double checked_at = -100;
    if (ImGui::GetTime() - checked_at > 2.0) { report = NewestReport(); checked_at = ImGui::GetTime(); }
    if (!report.empty()) {
      d->AddText(ui_font, 17, {o.x + 48, o.y + 452 + extra}, kGold,
                 tr("Un rapport de plantage ou de gel a été créé. Envoie-le avec ton signalement."));
      ImGui::SetCursorScreenPos({o.x + 48, o.y + 480 + extra});
      if (Tile("report", tr("OUVRIR LE RAPPORT"), {260, 44}, false, false, 18))
        ShellExecuteW(nullptr, L"open", report.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
  }
  d->AddText(ui_font, 15, {o.x + 48, o.y + area.y - 30}, kMuted,
             tr("Aucune donnée du jeu n'est incluse. Une copie légale du jeu est requise."));
}

bool Choice(const char* id, ImVec2 pos, const char* const* labels, const int* values, int count,
            int& value, float width = 110);

void DrawSettings(ImDrawList* d, ImVec2 o, ImVec2 area) {
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("RÉGLAGES"), tr("Dossier du jeu et options de lancement"));
  d->AddText(ui_font, 16, {o.x + 48, o.y + 112}, kMuted, tr("JEU XBOX 360 : FICHIER default.xex OU IMAGE ISO"));
  d->AddRectFilled({o.x + 48, o.y + 138}, {o.x + area.x - 48, o.y + 184}, IM_COL32(10, 10, 12, 255));
  const std::string path = game_data.empty() ? std::string(tr("Aucun jeu sélectionné")) : game_data.string();
  d->AddText(ui_font, 18, {o.x + 60, o.y + 150}, game_data.empty() ? kMuted : kWhite, path.c_str());
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 204});
  if (Tile("browse", tr("PARCOURIR…"), {210, 48}, false, true, 20)) BrowseGame();
  ImGui::SetCursorScreenPos({o.x + 280, o.y + 204});
  if (Tile("packages", tr("AJOUTER TU / DLC…"), {260, 48}, false, false, 20)) AddPackages();
  ImGui::SetCursorScreenPos({o.x + 562, o.y + 204});
  if (Tile("save", tr("ENREGISTRER"), {200, 48}, false, false, 20)) { SaveSettings(); status = tr("Réglages enregistrés."); }

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

// Graphics cards for the native renderer, named as DXGI reports them (the same names Vulkan uses).
const std::vector<std::string>& GpuNames() {
  static std::vector<std::string> names;
  static bool done = false;
  if (done) return names;
  done = true;
  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return names;
  ComPtr<IDXGIAdapter1> adapter;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i, adapter.Reset()) {
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
    const int n = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
    std::string name(n > 0 ? n - 1 : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name.data(), n, nullptr, nullptr);
    if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
  }
  return names;
}

// Graphics presets: 1 performance, 2 balanced, 3 quality; 0 = custom. Performance leaves FSI off
// (host render targets, about 35 % less GPU load); the others turn it on.
int CurrentPreset() {
  if (opt.resolution == 720 && opt.anisotropic == 0 && opt.aa == 0 && !opt.fsi) return 1;
  if (opt.resolution == 720 && opt.anisotropic == 3 && opt.aa == 0 && opt.fsi) return 2;
  if (opt.resolution == 1440 && opt.anisotropic == 5 && opt.aa == 1 && opt.fsi) return 3;
  return 0;
}

void ApplyPreset(int preset) {
  switch (preset) {
    case 1: opt.resolution = 720; opt.anisotropic = 0; opt.aa = 0; opt.fsi = false; break;
    case 2: opt.resolution = 720; opt.anisotropic = 3; opt.aa = 0; opt.fsi = true; break;
    case 3: opt.resolution = 1440; opt.anisotropic = 5; opt.aa = 1; opt.fsi = true; break;
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
  label(o.x + 48, y, tr("RÉSOLUTION"), UseNative()
        ? tr("AUTO : le rendu natif choisit selon la carte graphique et l'écran.")
        : tr("720p : l'image d'origine du jeu. Au-dessus : plus net, plus lourd pour la carte graphique."));
  {
    // The native renderer has its own choice: AUTO (it picks from the GPU and the screen) and 540p
    // (below the Xbox's own image); the Xenos choice is left alone.
    if (UseNative()) {
      const char* l[] = {tr("AUTO"), "540p", "720p", "1080p", "1440p", "4K"};
      static const int v[] = {0, 540, 720, 1080, 1440, 2160};
      if (Choice("nres", {o.x + 48, y + 46}, l, v, 6, opt.native_resolution, 78)) SaveOptions();
    } else {
      static const char* l[] = {"720p", "1080p", "1440p", "4K"};
      static const int v[] = {720, 1080, 1440, 2160};
      if (Choice("res", {o.x + 48, y + 46}, l, v, 4, opt.resolution, 110)) SaveOptions();
    }
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
  if (ImGui::Checkbox(tr("Synchronisation verticale (pas de déchirement)"), &opt.vsync)) SaveOptions();
  y += 34;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Rendu précis (FSI)"), &opt.fsi)) SaveOptions();
  const char* fsi_help = tr("Décoché : environ 35 % plus léger pour la carte graphique (conseillé pour les petites cartes, GTX 1650 et moins) ; les premières parties peuvent saccader le temps que le cache se remplisse.");
  const float fsi_wrap = o.x + area.x - 40 - (col2 + 28);
  d->AddText(ui_font, 14, {col2 + 28, y + 27}, kMuted, fsi_help, nullptr, fsi_wrap);
  y += 34 + ui_font->CalcTextSizeA(14, FLT_MAX, fsi_wrap, fsi_help).y;
  ImGui::SetCursorScreenPos({col2, y});
  if (ImGui::Checkbox(tr("Réduire l'utilisation du processeur"), &opt.cpu_yield)) SaveOptions();
  y += 36;
  ImGui::SetCursorScreenPos({col2, y});
  const bool native_present = NativeAvailable();
  ImGui::BeginDisabled(!native_present);
  if (ImGui::Checkbox(tr("Rendu natif (expérimental)"), &opt.native)) {
    SaveOptions();
  }
  ImGui::EndDisabled();
  d->AddText(ui_font, 14, {col2 + 28, y + 27}, kMuted,
             native_present ? tr("Plus léger pour les petites cartes. 1er lancement : shaders (quelques secondes).")
                            : tr("Rendu natif absent de cette version (wwe2k15_native.exe ou dxcompiler.dll manquant)."));
  if (UseNative()) {
    // A tile cycling through AUTO and the graphics cards found.
    const auto& names = GpuNames();
    ImGui::SetCursorScreenPos({col2 + 330, y - 2});
    const std::string label = std::string("GPU : ") + (opt.gpu.empty() ? tr("AUTO") : opt.gpu.c_str());
    if (Tile("gpu", label.c_str(), {area.x - (col2 - o.x) - 330 - 40, 26}, false, false, 14)) {
      size_t next = 0;  // 0 = AUTO, i + 1 = names[i]
      for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == opt.gpu) next = i + 1;
      next = next > names.size() ? 0 : (next + 1) % (names.size() + 1);
      opt.gpu = next == 0 ? std::string() : names[next - 1];
      SaveOptions();
    }
  }
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

std::string FormatSize(uint64_t bytes) {
  char buf[32];
  if (bytes >= (1ull << 20)) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
  else std::snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
  return buf;
}

void DrawSaves(ImDrawList* d, ImVec2 o, ImVec2 area) {
  static std::vector<saves::Backup> backups;
  static uint64_t current_bytes = 0;
  static int current_files = 0;
  static double refreshed_at = -100;
  const double now = ImGui::GetTime();
  if (now - refreshed_at > 3.0) {  // folder scans are not free; refresh every few seconds
    backups = saves::ListBackups(user_data);
    current_bytes = 0;
    current_files = 0;
    for (const auto& p : saves::ProfileFolders(user_data)) current_bytes += saves::FolderSize(p, &current_files);
    refreshed_at = now;
  }
  const bool running = game_process && WaitForSingleObject(game_process, 0) == WAIT_TIMEOUT;

  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("SAUVEGARDES"), tr("Copies de sécurité de tes parties"));
  const std::string cur = current_files
      ? std::string(tr("Sauvegardes actuelles : ")) + std::to_string(current_files) + tr(" fichier(s), ") + FormatSize(current_bytes)
      : std::string(tr("Aucune sauvegarde pour l'instant (elles apparaissent après la première partie)."));
  d->AddText(ui_font, 18, {o.x + 48, o.y + 108}, kWhite, cur.c_str());

  ImGui::SetCursorScreenPos({o.x + 48, o.y + 146});
  if (Tile("backup_now", tr("SAUVEGARDER MAINTENANT"), {300, 48}, false, true, 19)) {
    std::string err;
    status = saves::CreateBackup(user_data, err) ? tr("Copie de sécurité créée.")
             : err == "empty" ? tr("Rien à sauvegarder pour l'instant.") : std::string(tr("Échec de la copie : ")) + err;
    refreshed_at = -100;
  }
  ImGui::SetCursorScreenPos({o.x + 368, o.y + 146});
  if (Tile("saves_open", tr("OUVRIR LE DOSSIER"), {260, 48}, false, false, 19)) OpenFolder(user_data);
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 214});
  if (ImGui::Checkbox(tr("Copie automatique avant chaque lancement (garde les 10 dernières)"), &opt.auto_backup)) SaveOptions();

  d->AddText(ui_font, 18, {o.x + 48, o.y + 262}, kWhite, tr("COPIES DE SÉCURITÉ"));
  if (backups.empty()) {
    d->AddText(ui_font, 16, {o.x + 48, o.y + 292}, kMuted, tr("Aucune copie pour l'instant."));
  }
  float y = o.y + 292;
  for (size_t i = 0; i < backups.size() && i < 6; ++i) {
    const auto& b = backups[i];
    std::string when = b.name;  // yyyy-mm-dd_hh-mm-ss -> yyyy-mm-dd hh:mm:ss
    if (when.size() == 19) { when[10] = ' '; when[13] = ':'; when[16] = ':'; }
    const std::string line = when + "   " + FormatSize(b.bytes);
    d->AddText(ui_font, 17, {o.x + 48, y + 10}, kWhite, line.c_str());
    ImGui::SetCursorScreenPos({o.x + 420, y});
    ImGui::PushID(static_cast<int>(i));
    if (Tile("restore", tr("RESTAURER"), {170, 38}, false, false, 16)) {
      if (running) status = tr("Ferme le jeu avant de restaurer une sauvegarde.");
      else {
        std::string err;
        status = saves::RestoreBackup(user_data, b, err) ? tr("Sauvegarde restaurée.")
                                                          : std::string(tr("Échec de la restauration : ")) + err;
      }
      refreshed_at = -100;
    }
    ImGui::PopID();
    y += 44;
  }
  if (!status.empty()) d->AddText(ui_font, 17, {o.x + 48, o.y + area.y - 40}, kWhite, status.c_str());
}

// Newest crash/freeze report written since the last launch, if any.
fs::path NewestReport() {
  fs::path best;
  fs::file_time_type best_time{};
  std::error_code ec;
  if (launch_time == fs::file_time_type{}) return best;
  for (const auto& e : fs::directory_iterator(root / L"logs", ec)) {
    const std::wstring n = e.path().filename().wstring();
    if (e.path().extension() != L".txt" || (n.rfind(L"crash_", 0) != 0 && n.rfind(L"freeze_", 0) != 0)) continue;
    const auto t = e.last_write_time(ec);
    if (t >= launch_time && t > best_time) { best = e.path(); best_time = t; }
  }
  return best;
}

void DrawMods(ImDrawList* d, ImVec2 o, ImVec2 area) {
  static std::vector<std::string> files;
  static size_t total = 0;
  static double refreshed_at = -100;
  const fs::path mods = root / L"mods";
  if (ImGui::GetTime() - refreshed_at > 3.0) {
    files.clear();
    total = 0;
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(mods, ec)) {
      if (!e.is_regular_file(ec)) continue;
      const auto name = e.path().filename();
      if (name == L"LISEZMOI.txt" || name == L"README.txt") continue;
      ++total;
      if (files.size() < 10) files.push_back(fs::relative(e.path(), mods, ec).string());
    }
    refreshed_at = ImGui::GetTime();
  }
  Panel(d, o, {o.x + area.x, o.y + area.y});
  SectionTitle(d, {o.x + 40, o.y + 26}, tr("MODS"), tr("Remplace des fichiers du jeu sans le modifier"));
  d->AddText(ui_font, 16, {o.x + 48, o.y + 106}, kWhite,
             tr("Place un fichier dans le dossier mods au même chemin que dans le jeu, par exemple\nmods\\pac\\menu\\menu.pac : il remplace l'original au prochain lancement."),
             nullptr, area.x - 96);
  ImGui::SetCursorScreenPos({o.x + 48, o.y + 164});
  if (Tile("mods_open", tr("OUVRIR LE DOSSIER MODS"), {320, 48}, false, true, 19)) {
    std::error_code ec;
    fs::create_directories(mods, ec);
    const fs::path readme = mods / L"LISEZMOI.txt";
    if (!fs::exists(readme, ec)) {
      std::ofstream r(readme);
      r << "Place ici les fichiers qui remplacent ceux du jeu, au meme chemin.\n"
           "Exemple : mods\\pac\\menu\\menu.pac remplace pac\\menu\\menu.pac.\n"
           "Put replacement files here, at the same path as in the game.\n";
    }
    OpenFolder(mods);
  }
  ImGui::SetCursorScreenPos({o.x + 400, o.y + 176});
  if (ImGui::Checkbox(tr("Activer les mods"), &opt.mods)) SaveOptions();
  const std::string count = total ? std::to_string(total) + tr(" fichier(s) de mod trouvé(s) :") : std::string(tr("Aucun mod installé."));
  d->AddText(ui_font, 18, {o.x + 48, o.y + 236}, kWhite, count.c_str());
  float y = o.y + 266;
  for (const auto& f : files) {
    d->AddText(ui_font, 15, {o.x + 64, y}, kMuted, f.c_str());
    y += 22;
  }
  if (total > files.size()) d->AddText(ui_font, 15, {o.x + 64, y}, kMuted, "...");
  d->AddText(ui_font, 14, {o.x + 48, o.y + area.y - 36}, kMuted,
             tr("Ne partage pas de fichiers du jeu d'origine. Retire un mod en supprimant son fichier."));
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
    DrawMods(d, o, area);
  } else if (page == "SAUVEGARDES") {
    DrawSaves(d, o, area);
  } else {
    DrawTextPage(d, o, area, tr("À PROPOS"), "WWE 2K15 PC",
        tr("Portage PC non officiel de WWE 2K15, basé sur ReXGlue.\nAucune donnée du jeu n'est fournie.\n\nSans affiliation avec WWE, 2K ou Yuke's."));
    ImGui::SetCursorScreenPos({o.x + 48, o.y + 250});
    if (Tile("gh", tr("DÉPÔT GITHUB"), {250, 48}, false, true, 19))
      ShellExecuteW(nullptr, L"open", L"https://github.com/iMrBurn/wwe2k15-pc", nullptr, nullptr, SW_SHOWNORMAL);
    // Updates in the right column, scrolling when the release notes are long.
    const float col = o.x + area.x * 0.5f + 16;
    d->AddText(ui_font, 18, {col, o.y + 104}, kWhite, tr("MISES À JOUR"));
    const std::string current = std::string(tr("Version installée : ")) + WWE2K15_VERSION;
    d->AddText(ui_font, 15, {col, o.y + 128}, kMuted, current.c_str());
    ImGui::SetCursorScreenPos({col, o.y + 158});
    ImGui::BeginChild("##updates", {area.x * 0.5f - 64, area.y - 190});
    const UpdaterTexts texts{tr("Recherche d'une mise à jour..."), tr("Tu as la dernière version."),
                             tr("Nouvelle version disponible :"), tr("INSTALLER LA MISE À JOUR"),
                             tr("Impossible de vérifier les mises à jour (pas d'internet ?)."),
                             tr("Vérifier maintenant"), tr("Vérifier au démarrage"),
                             tr("Téléchargement..."), tr("La mise à jour a échoué :"),
                             tr("Inclure les versions bêta"),
                             tr("Ferme le jeu avant d'installer la mise à jour.")};
    updater.Draw(texts);
    ImGui::EndChild();
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
  updater.Start(root);
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
    if (updater.RestartRequested()) done = true;  // the updated launcher is starting
    if(done) break;
    // Idle cheaply: nothing to draw while minimised, and only a slow refresh while the game runs.
    if (IsIconic(hwnd)) { Sleep(150); continue; }
    if (game_process) Sleep(60);
    if(resize_w && resize_h) { target.Reset(); swapchain->ResizeBuffers(0,resize_w,resize_h,DXGI_FORMAT_UNKNOWN,0); resize_w=resize_h=0; MakeTarget(); }
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); PollPad(); ImGui::NewFrame(); DrawUi(); ImGui::Render();
    const float bg[]={.035f,.035f,.04f,1}; context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr); context->ClearRenderTargetView(target.Get(),bg);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); swapchain->Present(1,0);
  }
  updater.Shutdown();
  ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
  target.Reset(); swapchain.Reset(); context.Reset(); device.Reset(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName,wc.hInstance);
  if (game_process) CloseHandle(game_process);
  if (pad) SDL_CloseGamepad(pad);
  SDL_Quit();
  CoUninitialize();
  return 0;
}
