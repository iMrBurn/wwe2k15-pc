// wwe2k15 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging/macros.h>
#include <rex/rex_app.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>
#include <rex/ui/imgui_dialog.h>

#include <atomic>
#include <imgui.h>

#if defined(SVR_NATIVE_RENDERER)
// Native renderer (wwe2k15_native.exe only, cmake/native_renderer.cmake): re:Blue draws through
// plume (Vulkan) from the game's own D3D calls instead of emulating the Xenos GPU.
#include <rex/system/gpu_plugin.h>
#include "gpu/device.h"
#include "gpu/imgui_overlay_drawer.h"
#endif

#include <rex/cvar.h>

REXCVAR_DECLARE(bool, wwe2k15_show_fps);
REXCVAR_DECLARE(bool, wwe2k15_mods);
void InstallWwe2k15CrashReporter();
extern std::atomic<uint32_t> g_wwe2k15_fps_x10;
extern std::atomic<uint32_t> g_wwe2k15_worst_frame_us;

// Small FPS counter in the top-left corner (cvar wwe2k15_show_fps). It never takes input.
class FpsCounterDialog : public rex::ui::ImGuiDialog {
 public:
  explicit FpsCounterDialog(rex::ui::ImGuiDrawer* drawer) : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO&) override {
    const float fps = g_wwe2k15_fps_x10.load() / 10.0f;
    const float worst_ms = g_wwe2k15_worst_frame_us.load() / 1000.0f;
    if (fps <= 0) return;
    ImGui::SetNextWindowPos(ImVec2(12, 12));
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##wwe2k15_fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    const ImVec4 color = fps >= 55 ? ImVec4(0.45f, 0.85f, 0.35f, 1) :
                         fps >= 28 ? ImVec4(0.95f, 0.8f, 0.3f, 1) : ImVec4(0.95f, 0.35f, 0.3f, 1);
    ImGui::TextColored(color, "%.0f FPS", fps);
    ImGui::Text("%.1f ms max", worst_ms);
    ImGui::End();
  }
};

class Wwe2k15App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Wwe2k15App>(new Wwe2k15App(ctx, "wwe2k15",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
#if defined(_WIN32)
    // The GPU command processor and VSync worker wait with Sleep(1 ms). At the default
    // ~15.6 ms timer tick every such wait costs a whole tick, which halves the frame rate
    // in scenes (movies) that hit those waits, so ask for a 1 ms timer resolution.
    timeBeginPeriod(1);
#endif
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
#if defined(SVR_NATIVE_RENDERER)
    // The Xenos plugin's "null" backend keeps the game's D3D command stream moving (fences,
    // interrupts, vblank) without drawing; the re:Blue hooks draw instead.
    if (!config.graphics) {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, "null");
      REXLOG_INFO("Native renderer: guest GPU {}", config.graphics ? "null (sync only)" : "MISSING");
    }
#endif
  }

#if defined(SVR_NATIVE_RENDERER)
  // The plume device draws the ImGui overlays at Present time.
  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    return std::make_unique<bd::gpu::ImGuiOverlayDrawer>();
  }

  void OnPreLaunchModule() override {
    if (!bd::gpu::Video::CreateHostDevice(window())) {
      REXLOG_ERROR("Native renderer: could not create the host device (Vulkan unavailable?)");
      std::_Exit(1);
    }
    // Present runs on the game thread and ImGui on the UI thread: draw the overlay
    // synchronously on the UI thread into the presented image.
    bd::gpu::Video::SetOverlayDrawHook([this](plume::RenderCommandList* cmd,
                                              plume::RenderFramebuffer* fb, uint32_t w,
                                              uint32_t h) {
      if (!imgui_drawer() || !imgui_drawer()->HasDialogs())
        return;
      app_context().CallInUIThreadSynchronous([this, cmd, fb, w, h] {
        bd::gpu::ReblueUIDrawContext ctx(w, h, cmd, fb);
        imgui_drawer()->Draw(ctx);
      });
    });
  }

  void OnWindowPixelSizeChanged(uint32_t, uint32_t) override { bd::gpu::Video::RequestResize(); }
#endif

  void OnConfigurePaths(rex::PathConfig& paths) override {
    const auto exe_dir = rex::filesystem::GetExecutableFolder();

    // Keep graphics/pipeline caches beside this portable build. The default
    // Documents cache folder may be read-only in managed or restricted setups,
    // which disables Vulkan's persistent shader storage and forces recompiles.
    paths.cache_root = exe_dir / "cache";

    if (paths.game_data_root.empty()) {
      std::array<std::filesystem::path, 8> candidates = {
          exe_dir / "assets",
          exe_dir / "game",
          exe_dir,
          exe_dir / ".." / ".." / ".." / ".." / "assets",
          exe_dir / ".." / ".." / ".." / ".." / "game",
          {},
          {},
          {},
      };
      if (const char* user_profile = std::getenv("USERPROFILE")) {
        candidates[5] = std::filesystem::path(user_profile) / "Desktop" / "WWE2K15";
        candidates[6] = std::filesystem::path(user_profile) / "Documents" / "WWE2K15";
      }
      if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        candidates[7] = std::filesystem::path(local_app_data) / "WWE2K15" / "game";
      }

      for (const auto& candidate : candidates) {
        std::error_code ec;
        // The generated code is built from the TU-patched executable, so only a
        // folder that also holds the title update's default.xexp is usable.
        if (!candidate.empty() && std::filesystem::is_regular_file(candidate / "default.xex", ec) &&
            std::filesystem::is_regular_file(candidate / "default.xexp", ec)) {
          paths.game_data_root = std::filesystem::weakly_canonical(candidate, ec);
          if (ec) {
            paths.game_data_root = candidate;
          }
          REXLOG_INFO("Using game data from {}", paths.game_data_root.string());
          break;
        }
      }
      // Or a disc image (.iso) in game\ or beside the executable, read in place; its title
      // update and DLC then live in game_update\ beside the executable.
      for (const auto& dir : {exe_dir / "game", exe_dir}) {
        if (!paths.game_data_root.empty()) break;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
          if (entry.is_regular_file(ec) && IsDiscImage(entry.path()) &&
              std::filesystem::is_regular_file(exe_dir / "game_update" / "default.xexp", ec)) {
            paths.game_data_root = entry.path();
            REXLOG_INFO("Using the disc image {}", paths.game_data_root.string());
            break;
          }
        }
      }
      if (paths.game_data_root.empty()) {
        REXLOG_ERROR("Could not find default.xex with its title update (default.xexp). Put the game folder beside the executable as 'assets' or 'game', or under Desktop\\WWE2K15.");
      }
    } else if (!IsDiscImage(paths.game_data_root)) {
      std::error_code ec;
      if (!std::filesystem::is_regular_file(paths.game_data_root / "default.xexp", ec)) {
        REXLOG_ERROR("{} has no default.xexp: this build needs WWE 2K15 Title Update 4 beside default.xex.",
                     paths.game_data_root.string());
      }
    }

    if (paths.user_data_root.empty()) {
      if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        paths.user_data_root = std::filesystem::path(local_app_data) / "WWE2K15";
      } else {
        paths.user_data_root = exe_dir / "user_data";
      }
    }
    user_data_root_ = paths.user_data_root;
    game_data_root_ = paths.game_data_root;
    // Title update and DLC: beside default.xex in a game folder; a disc image cannot hold them,
    // so they go in game_update\ beside the executable (the launcher installs them there).
    extras_root_ = IsDiscImage(game_data_root_) ? exe_dir / "game_update" : game_data_root_;
    if (IsDiscImage(game_data_root_)) {
      std::error_code ec;
      if (!std::filesystem::is_regular_file(extras_root_ / "default.xexp", ec)) {
        REXLOG_ERROR("{} has no default.xexp: this build needs WWE 2K15 Title Update 4 (add it from the launcher).",
                     extras_root_.string());
      }
    }
  }

  // With a disc image, default.xexp is not beside default.xex on the disc: game_update\ is laid
  // over the disc (as the mods folder is) before the executable loads, so the loader finds
  // game:\default.xexp there and applies the title update.
  void OnLoadXexImage(std::string& xex_image) override {
    if (!IsDiscImage(game_data_root_)) return;
    // Overlays are searched in registration order: mods go first so they win over game_update.
    MountMods();
    auto* fs = REX_KERNEL_FS();
    auto device = std::make_unique<rex::filesystem::HostPathDevice>("\\GAMEUPDATE", extras_root_, true);
    if (!fs || !device->Initialize() || !fs->RegisterDevice(std::move(device))) {
      REXLOG_ERROR("Unable to mount {} for the disc image", extras_root_.string());
      return;
    }
    fs->RegisterOverlay("\\Device\\Harddisk0\\Partition1", "\\GAMEUPDATE");
    REXLOG_INFO("Disc image: title update read from {}", extras_root_.string());
  }

  // The title reads and writes its Xbox HDD cache (for example EditEx.cac).
  // ReXGlue does not mount cache: by default, so give it a writable local folder.
  void OnPostSetup() override {
    InstallWwe2k15CrashReporter();
    MountTitleUpdate();
    MountMods();
    InstallDlcPackages();

    if (user_data_root_.empty()) {
      REXLOG_WARN("Xbox cache is unavailable because user_data_root is empty");
      return;
    }

    const auto cache_root = user_data_root_ / "xbox_cache";
    std::error_code ec;
    std::filesystem::create_directories(cache_root, ec);
    if (ec) {
      REXLOG_ERROR("Unable to create Xbox cache folder {}: {}",
                   cache_root.string(), ec.message());
      return;
    }

    auto* fs = REX_KERNEL_FS();
    auto device = std::make_unique<rex::filesystem::HostPathDevice>(
        "\\CACHE", cache_root, false);
    if (!fs || !device->Initialize() || !fs->RegisterDevice(std::move(device))) {
      REXLOG_ERROR("Unable to mount Xbox cache folder {}", cache_root.string());
      return;
    }

    fs->RegisterSymbolicLink("cache:", "\\CACHE");
    fs->RegisterSymbolicLink("cache0:", "\\CACHE");
    fs->RegisterSymbolicLink("cache1:", "\\CACHE");
    REXLOG_INFO("Xbox cache mounted at {}", cache_root.string());
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    if (REXCVAR_GET(wwe2k15_show_fps)) {
      fps_counter_ = std::make_unique<FpsCounterDialog>(drawer);
    }
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  void OnShutdown() override { fps_counter_.reset(); }
 private:
  // Title Update data. The TU's default.xexp sits beside default.xex (the loader
  // applies it, and the generated code is built from the patched image); the rest
  // of the TU package (pac/, plist360.*) is read by the title through UPDATE:.
  void MountTitleUpdate() {
    const auto update_root = extras_root_ / "update";
    std::error_code ec;
    if (extras_root_.empty() || !std::filesystem::is_directory(update_root, ec)) {
      REXLOG_WARN("No title update data folder at {}", update_root.string());
      return;
    }
    auto* fs = REX_KERNEL_FS();
    auto device = std::make_unique<rex::filesystem::HostPathDevice>(
        "\\UPDATE", update_root, true);
    if (!fs || !device->Initialize() || !fs->RegisterDevice(std::move(device))) {
      REXLOG_ERROR("Unable to mount title update folder {}", update_root.string());
      return;
    }
    fs->RegisterSymbolicLink("update:", "\\UPDATE");
    REXLOG_INFO("Title update mounted at {}", update_root.string());
  }

  // Mods: files in <exe>\mods replace the game's files at the same relative path (for example
  // mods\pac\menu\menu.pac), for both GAME: and the title update's UPDATE:. Read-only; the
  // game folder itself is never modified. Disabled with --wwe2k15_mods=false.
  void MountMods() {
    if (!REXCVAR_GET(wwe2k15_mods) || mods_mounted_) return;
    const auto mods = rex::filesystem::GetExecutableFolder() / "mods";
    std::error_code ec;
    size_t files = 0;
    for (const auto& e : std::filesystem::recursive_directory_iterator(mods, ec))
      if (e.is_regular_file(ec) && e.path().filename() != "LISEZMOI.txt" && e.path().filename() != "README.txt") ++files;
    if (!files) return;
    auto* fs = REX_KERNEL_FS();
    auto device = std::make_unique<rex::filesystem::HostPathDevice>("\\MODS", mods, true);
    if (!fs || !device->Initialize() || !fs->RegisterDevice(std::move(device))) {
      REXLOG_ERROR("Unable to mount mods folder {}", mods.string());
      return;
    }
    mods_mounted_ = true;
    fs->RegisterOverlay("\\Device\\Harddisk0\\Partition1", "\\MODS");
    fs->RegisterOverlay("\\UPDATE", "\\MODS");
    REXLOG_INFO("Mods enabled: {} file(s) from {}", files, mods.string());
  }

  // DLC: STFS packages (as copied from the console, e.g. 545408B6\00000002\*) go in
  // the game folder's dlc\ subfolder. Each one not yet installed is extracted by the
  // SDK's content manager into user data, where XContent enumeration finds it.
  void InstallDlcPackages() {
    using rex::X_RESULT;
    const auto dlc_root = extras_root_ / "dlc";
    std::error_code ec;
    if (extras_root_.empty() || !std::filesystem::is_directory(dlc_root, ec)) {
      return;
    }
    auto* kernel_state = REX_KERNEL_STATE();
    auto* content = kernel_state ? kernel_state->content_manager() : nullptr;
    if (!content || user_data_root_.empty()) {
      REXLOG_ERROR("Cannot install DLC: content manager or user data folder unavailable");
      return;
    }
    const auto installed_root = user_data_root_ / "0000000000000000" /
                                fmt::format("{:08X}", kernel_state->title_id()) / "00000002";
    for (const auto& entry : std::filesystem::directory_iterator(dlc_root, ec)) {
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      const auto name = entry.path().filename();
      if (std::filesystem::is_directory(installed_root / name, ec)) {
        continue;
      }
      REXLOG_INFO("Installing DLC package {} (first launch only)", name.string());
      const rex::X_RESULT result = content->InstallContent(entry.path());
      if (result != X_ERROR_SUCCESS) {
        REXLOG_ERROR("DLC package {} failed to install: {:08X}", name.string(), result);
        std::filesystem::remove_all(installed_root / name, ec);
      }
    }
  }

  static bool IsDiscImage(const std::filesystem::path& p) {
    std::error_code ec;
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".iso" && std::filesystem::is_regular_file(p, ec);
  }

  std::filesystem::path user_data_root_;
  std::filesystem::path game_data_root_;
  bool mods_mounted_ = false;
  std::filesystem::path extras_root_;  // holds default.xexp, the update folder and the dlc folder
  std::unique_ptr<FpsCounterDialog> fps_counter_;
};
