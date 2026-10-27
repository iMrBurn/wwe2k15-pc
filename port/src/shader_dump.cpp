// wwe2k15 - shader container dump (native renderer bring-up tool, off by default)
//
// WWE 2K15's shaders sit compressed in its archives (no raw 0x102A11xx container is found in the
// game files, unlike WWE '12's BPE .pac). The game creates every shader at run time through the XDK
// D3DDevice_CreateVertexShader / CreatePixelShader (sub_82BABB30 / sub_82BAB948 in the TU4 code,
// see src/native/wwe2k15_d3d_names.h), so this hook saves the container each call is given, for
// the offline XenosRecomp conversion. With wwe2k15_dump_shaders = true, every distinct container
// is written once as shaders/xsc/<xxh-like hash>.<vs|ps>.xsc next to the executable. The files come
// from the user's own game data: never commit or ship them.

#include "generated/default/wwe2k15_init.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <unordered_set>

REXCVAR_DEFINE_BOOL(wwe2k15_dump_shaders, false, "WWE2K15",
                    "Save every shader container the game creates to shaders/xsc (dev tool)");

REX_EXTERN(__imp__sub_82BABB30);  // D3DDevice_CreateVertexShader
REX_EXTERN(__imp__sub_82BAB948);  // D3DDevice_CreatePixelShader

namespace {

constexpr uint32_t kHeaderSize = 0x24;
constexpr uint32_t kMaxSize = 0x100000;

std::mutex g_mutex;
std::unordered_set<uint64_t> g_seen;
std::atomic<uint32_t> g_written{0};

uint32_t LoadBe32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

uint64_t Fnv1a(const uint8_t* data, size_t size) {
  uint64_t hash = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < size; ++i) {
    hash = (hash ^ data[i]) * 0x100000001B3ull;
  }
  return hash;
}

// Looks for a shader container (big-endian header: flags 0x102A11xx, two part sizes whose sum is
// the container size, zeros at +0x1C and +0x20) at any guest pointer the call was given.
void DumpFrom(const PPCContext& ctx, uint8_t* base, bool vertex) {
  const uint32_t candidates[] = {ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32};
  for (uint32_t address : candidates) {
    if (address < 0x10000 || address >= 0xFFF00000u) {
      continue;
    }
    const uint8_t* p = base + address;
    const uint32_t flags = LoadBe32(p);
    if ((flags & 0xFFFFFF00u) != 0x102A1100u) {
      continue;
    }
    const uint32_t size = LoadBe32(p + 4) + LoadBe32(p + 8);
    if (size <= kHeaderSize || size > kMaxSize || LoadBe32(p + 0x1C) || LoadBe32(p + 0x20)) {
      continue;
    }
    const uint64_t hash = Fnv1a(p, size);
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (!g_seen.insert(hash).second) {
        return;
      }
    }
    const auto dir = rex::filesystem::GetExecutableFolder() / "shaders" / "xsc";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char name[64];
    std::snprintf(name, sizeof(name), "%016llX.%s.xsc", static_cast<unsigned long long>(hash),
                  (flags & 1) ? "vs" : "ps");
    if (FILE* file = std::fopen((dir / name).string().c_str(), "wb")) {
      std::fwrite(p, 1, size, file);
      std::fclose(file);
      const uint32_t written = ++g_written;
      if (written % 100 == 0) {
        REXLOG_INFO("shader dump: {} containers saved", written);
      }
    }
    return;
  }
  static std::atomic<uint32_t> misses{0};
  if (misses++ < 8) {
    REXLOG_WARN("shader dump: no container found for a Create{}Shader call (r3={:08X} r4={:08X} "
                "r5={:08X} r6={:08X})",
                vertex ? "Vertex" : "Pixel", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  }
}

}  // namespace

extern "C" REX_FUNC(sub_82BABB30) {
  if (REXCVAR_GET(wwe2k15_dump_shaders)) {
    DumpFrom(ctx, base, true);
  }
  __imp__sub_82BABB30(ctx, base);
}

extern "C" REX_FUNC(sub_82BAB948) {
  if (REXCVAR_GET(wwe2k15_dump_shaders)) {
    DumpFrom(ctx, base, false);
  }
  __imp__sub_82BAB948(ctx, base);
}
