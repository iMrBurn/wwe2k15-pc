// wwe2k15 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>
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
  }

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
      if (paths.game_data_root.empty()) {
        REXLOG_ERROR("Could not find default.xex with its title update (default.xexp). Put the game folder beside the executable as 'assets' or 'game', or under Desktop\\WWE2K15.");
      }
    } else {
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
  }

  // The title reads and writes its Xbox HDD cache (for example EditEx.cac).
  // ReXGlue does not mount cache: by default, so give it a writable local folder.
  void OnPostSetup() override {
    MountTitleUpdate();
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
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
 private:
  // Title Update data. The TU's default.xexp sits beside default.xex (the loader
  // applies it, and the generated code is built from the patched image); the rest
  // of the TU package (pac/, plist360.*) is read by the title through UPDATE:.
  void MountTitleUpdate() {
    const auto update_root = game_data_root_ / "update";
    std::error_code ec;
    if (game_data_root_.empty() || !std::filesystem::is_directory(update_root, ec)) {
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

  // DLC: STFS packages (as copied from the console, e.g. 545408B6\00000002\*) go in
  // the game folder's dlc\ subfolder. Each one not yet installed is extracted by the
  // SDK's content manager into user data, where XContent enumeration finds it.
  void InstallDlcPackages() {
    using rex::X_RESULT;
    const auto dlc_root = game_data_root_ / "dlc";
    std::error_code ec;
    if (game_data_root_.empty() || !std::filesystem::is_directory(dlc_root, ec)) {
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

  std::filesystem::path user_data_root_;
  std::filesystem::path game_data_root_;
};
