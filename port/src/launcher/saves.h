// Save-data backups for the launcher.
//
// Game saves live in the user data folder under one folder per profile XUID (16 hex digits,
// for example B13EBABEBABEBABE\545408B6\00000001). 0000000000000000 holds installed DLC and
// xbox_cache the HDD cache: neither is save data. A backup copies every profile folder into
// <user data>\backups\<yyyy-mm-dd_hh-mm-ss>\.
#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace saves {
namespace fs = std::filesystem;

inline bool IsProfileFolder(const fs::path& p) {
  const std::wstring n = p.filename().wstring();
  if (n.size() != 16 || n == L"0000000000000000") return false;
  return std::all_of(n.begin(), n.end(), [](wchar_t c) { return iswxdigit(c) != 0; });
}

inline std::vector<fs::path> ProfileFolders(const fs::path& user_data) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(user_data, ec))
    if (e.is_directory(ec) && IsProfileFolder(e.path())) out.push_back(e.path());
  return out;
}

inline uint64_t FolderSize(const fs::path& p, int* files = nullptr) {
  uint64_t total = 0;
  std::error_code ec;
  for (const auto& e : fs::recursive_directory_iterator(p, ec)) {
    if (e.is_regular_file(ec)) {
      total += e.file_size(ec);
      if (files) ++*files;
    }
  }
  return total;
}

inline bool HasSaves(const fs::path& user_data) {
  for (const auto& p : ProfileFolders(user_data))
    if (FolderSize(p) > 0) return true;
  return false;
}

struct Backup {
  fs::path path;
  std::string name;  // yyyy-mm-dd_hh-mm-ss
  uint64_t bytes = 0;
};

inline fs::path BackupRoot(const fs::path& user_data) { return user_data / L"backups"; }

inline std::vector<Backup> ListBackups(const fs::path& user_data) {
  std::vector<Backup> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(BackupRoot(user_data), ec)) {
    if (!e.is_directory(ec)) continue;
    out.push_back({e.path(), e.path().filename().string(), FolderSize(e.path())});
  }
  std::sort(out.begin(), out.end(), [](const Backup& a, const Backup& b) { return a.name > b.name; });
  return out;
}

inline std::string Timestamp() {
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &t);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
  return buf;
}

// Copies every profile folder into a new backup. Returns false when there is nothing to save.
inline bool CreateBackup(const fs::path& user_data, std::string& error) {
  const auto profiles = ProfileFolders(user_data);
  if (profiles.empty() || !HasSaves(user_data)) { error = "empty"; return false; }
  const fs::path dst = BackupRoot(user_data) / Timestamp();
  std::error_code ec;
  fs::create_directories(dst, ec);
  for (const auto& p : profiles) {
    fs::copy(p, dst / p.filename(), fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) { error = ec.message(); return false; }
  }
  return true;
}

// Keeps the newest `keep` backups.
inline void PruneBackups(const fs::path& user_data, size_t keep) {
  auto all = ListBackups(user_data);
  std::error_code ec;
  for (size_t i = keep; i < all.size(); ++i) fs::remove_all(all[i].path, ec);
}

// Replaces the current profile folders with the backup's (current saves are backed up first).
inline bool RestoreBackup(const fs::path& user_data, const Backup& b, std::string& error) {
  std::string ignored;
  CreateBackup(user_data, ignored);
  std::error_code ec;
  for (const auto& p : ProfileFolders(user_data)) fs::remove_all(p, ec);
  for (const auto& e : fs::directory_iterator(b.path, ec)) {
    if (!e.is_directory(ec) || !IsProfileFolder(e.path())) continue;
    fs::copy(e.path(), user_data / e.path().filename(),
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) { error = ec.message(); return false; }
  }
  return true;
}

}  // namespace saves
