// wwe2k15 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging/macros.h>
#include <rex/rex_app.h>
#include <rex/system/kernel_state.h>

class Wwe2k15App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Wwe2k15App>(new Wwe2k15App(ctx, "wwe2k15",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
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
        if (!candidate.empty() && std::filesystem::is_regular_file(candidate / "default.xex", ec)) {
          paths.game_data_root = std::filesystem::weakly_canonical(candidate, ec);
          if (ec) {
            paths.game_data_root = candidate;
          }
          REXLOG_INFO("Using game data from {}", paths.game_data_root.string());
          break;
        }
      }
      if (paths.game_data_root.empty()) {
        REXLOG_ERROR("Could not find default.xex. Put the game folder beside the executable as 'assets' or 'game', or under Desktop\\WWE2K15.");
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
  }

  // The title reads and writes its Xbox HDD cache (for example EditEx.cac).
  // ReXGlue does not mount cache: by default, so give it a writable local folder.
  void OnPostSetup() override {
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
  std::filesystem::path user_data_root_;
};
