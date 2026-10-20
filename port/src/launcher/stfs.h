// Minimal read-only reader for Xbox 360 STFS packages (LIVE / PIRS / CON).
// Used by the launcher to install a title update or DLC package the user owns.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace stfs {

constexpr uint32_t kTypeTitleUpdate = 0x000B0000;
constexpr uint32_t kTypeMarketplace = 0x00000002;

struct Entry {
  std::string name;
  bool directory = false;
  bool contiguous = false;
  uint32_t blocks = 0;
  uint32_t start = 0;
  int16_t parent = -1;
  uint32_t size = 0;
};

class Package {
 public:
  bool Open(const std::filesystem::path& path) {
    file_.open(path, std::ios::binary);
    if (!file_) return false;
    uint8_t h[0x1000]{};
    file_.read(reinterpret_cast<char*>(h), sizeof(h));
    if (!file_) return false;
    const std::string magic(reinterpret_cast<char*>(h), 4);
    if (magic != "LIVE" && magic != "PIRS" && magic != "CON ") return false;
    const uint32_t header_size = be32(h + 0x340);
    content_type = be32(h + 0x344);
    title_id = be32(h + 0x360);
    version = be32(h + 0x358);
    for (int i = 0; i < 0x40; ++i) {  // UTF-16BE display name, ASCII part only
      const uint16_t c = uint16_t(h[0x411 + i * 2] << 8 | h[0x412 + i * 2]);
      if (!c) break;
      display_name.push_back(c < 0x80 ? char(c) : '?');
    }
    const uint8_t* vd = h + 0x379;
    sex_ = (~vd[2]) & 1;
    ft_blocks_ = uint32_t(vd[3] | vd[4] << 8);
    ft_start_ = uint32_t(vd[5] | vd[6] << 8 | vd[7] << 16);
    first_hash_ = (header_size + 0xFFF) & 0xFFFFF000;
    return ReadFileTable();
  }

  std::string PathOf(size_t index) const {
    std::string path = entries[index].name;
    for (int16_t p = entries[index].parent; p >= 0 && size_t(p) < entries.size(); p = entries[p].parent)
      path = entries[p].name + "/" + path;
    return path;
  }

  bool Extract(const Entry& e, const std::filesystem::path& dst) {
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    uint32_t remaining = e.size, block = e.start;
    std::vector<char> buf(0x1000);
    for (uint32_t i = 0; i < e.blocks && remaining; ++i) {
      if (!ReadAt(BlockOffset(block), buf.data(), buf.size())) return false;
      const uint32_t n = remaining < 0x1000 ? remaining : 0x1000;
      out.write(buf.data(), n);
      remaining -= n;
      block = e.contiguous ? block + 1 : NextBlock(block);
    }
    return remaining == 0 && bool(out);
  }

  uint32_t content_type = 0, title_id = 0, version = 0;
  std::string display_name;
  std::vector<Entry> entries;

 private:
  static uint32_t be32(const uint8_t* p) { return uint32_t(p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]); }

  uint64_t BlockOffset(uint32_t b) const {
    uint64_t r = (uint64_t((b + 0xAA) / 0xAA) << sex_) + b;
    if (b >= 0xAA) r += uint64_t((b + 0x70E4) / 0x70E4) << sex_;
    if (b >= 0x70E4) r += uint64_t(1) << sex_;
    return (r << 12) + first_hash_;
  }

  uint32_t NextBlock(uint32_t b) {
    uint64_t n = uint64_t(b / 0xAA) * (sex_ ? 0xAC : 0xAB);
    if (b >= 0xAA) {
      n += uint64_t((b / 0x70E4) + 1) << sex_;
      if (b >= 0x70E4) n += uint64_t(1) << sex_;
    }
    const uint64_t off = (n << 12) + first_hash_ + uint64_t(b % 0xAA) * 0x18;
    uint8_t e[0x18]{}, alt[0x18]{};
    ReadAt(off, e, sizeof(e));
    if (sex_) {  // two table copies: prefer the one whose status bits mark it active
      ReadAt(off + 0x1000, alt, sizeof(alt));
      if ((alt[0x14] & 0xC0) && !(e[0x14] & 0xC0)) return uint32_t(alt[0x15] << 16 | alt[0x16] << 8 | alt[0x17]);
    }
    return uint32_t(e[0x15] << 16 | e[0x16] << 8 | e[0x17]);
  }

  bool ReadAt(uint64_t off, void* dst, size_t n) {
    file_.clear();
    file_.seekg(std::streamoff(off));
    file_.read(static_cast<char*>(dst), std::streamsize(n));
    return bool(file_);
  }

  bool ReadFileTable() {
    std::vector<uint8_t> raw(size_t(ft_blocks_) * 0x1000);
    uint32_t block = ft_start_;
    for (uint32_t i = 0; i < ft_blocks_; ++i) {
      if (!ReadAt(BlockOffset(block), raw.data() + size_t(i) * 0x1000, 0x1000)) return false;
      block = NextBlock(block);
    }
    for (size_t i = 0; i + 0x40 <= raw.size(); i += 0x40) {
      const uint8_t* e = raw.data() + i;
      const uint8_t len = e[0x28] & 0x3F;
      if (!len) break;
      Entry entry;
      entry.name.assign(reinterpret_cast<const char*>(e), len);
      entry.directory = (e[0x28] & 0x80) != 0;
      entry.contiguous = (e[0x28] & 0x40) != 0;
      entry.blocks = uint32_t(e[0x29] | e[0x2A] << 8 | e[0x2B] << 16);
      entry.start = uint32_t(e[0x2F] | e[0x30] << 8 | e[0x31] << 16);
      entry.parent = int16_t(e[0x32] << 8 | e[0x33]);
      entry.size = be32(e + 0x34);
      entries.push_back(entry);
    }
    return !entries.empty();
  }

  std::ifstream file_;
  uint32_t sex_ = 0, ft_blocks_ = 0, ft_start_ = 0;
  uint64_t first_hash_ = 0;
};

}  // namespace stfs
