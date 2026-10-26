// wwe2k15 - "Xbox 360" in the game's texts becomes "PC"
//
// The game's texts (pac\string.pac, update\pac\string_pf.pac) are UTF-8, in zlib blocks the
// game inflates itself. sub_82B529A0 is the title's zlib inflate (TU4 address; it calls adler32
// sub_82B69DA8 and references "incorrect header check"). After each call, the bytes it wrote
// (z_stream next_out before/after) are scanned for the console warnings of the five languages
// ("Please don't turn off your Xbox 360 console.") and rewritten in place. Replacements are
// shorter than what they replace: the rest of the string moves left and the freed bytes before
// its terminating NUL become NULs, so string offsets and table sizes stay valid.
// Xbox Live / Xbox Store texts (online features) are left as they are.

#include "generated/default/wwe2k15_init.h"

#include <atomic>
#include <cstring>
#include <string_view>

#include <rex/cvar.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_pc_texts, true, "WWE2K15",
                    "Say PC instead of Xbox 360 console in the game's texts");

REX_EXTERN(__imp__sub_82B529A0);

namespace {

struct Rule {
  std::string_view from, to;
};

// Longest first: each language's whole phrase, then the bare names.
constexpr Rule kRules[] = {
    {"el sistema Consola Xbox 360", "tu PC"},     // es
    {"your Xbox 360 console", "your PC"},         // en
    {"votre Console Xbox 360", "votre PC"},       // fr
    {"votre console Xbox 360", "votre PC"},       // fr
    {"deine Xbox 360 Konsole", "deinen PC"},      // de
    {"tu Consola Xbox 360", "tu PC"},             // es
    {"la console Xbox 360", "il PC"},             // it
    {"Xbox 360 console", "PC"},
    {"Xbox 360 Konsole", "PC"},
    {"Consola Xbox 360", "PC"},
    {"Console Xbox 360", "PC"},
    {"console Xbox 360", "PC"},
    {"Xbox 360", "PC"},
};
constexpr std::string_view kKey = "Xbox 360";

// Rewrites the text strings in [p, end). Only touches a match whose string ends (NUL) in range.
// Returns the number of strings changed.
int Rewrite(char* p, char* end) {
  int changed = 0;
  char* hit = p;
  while (end - hit >= static_cast<ptrdiff_t>(kKey.size())) {
    hit = static_cast<char*>(std::memchr(hit, 'X', end - hit));
    if (!hit || end - hit < static_cast<ptrdiff_t>(kKey.size())) return changed;
    if (std::memcmp(hit, kKey.data(), kKey.size()) != 0) { ++hit; continue; }
    char* nul = static_cast<char*>(std::memchr(hit, 0, end - hit));
    if (!nul) return changed;
    // Try every rule whose "from" contains this occurrence of the key.
    bool done = false;
    for (const Rule& r : kRules) {
      const size_t key_at = r.from.find(kKey);
      if (hit - p < static_cast<ptrdiff_t>(key_at)) continue;
      char* start = hit - key_at;
      if (nul - start < static_cast<ptrdiff_t>(r.from.size()) ||
          std::memcmp(start, r.from.data(), r.from.size()) != 0)
        continue;
      char* tail = start + r.from.size();
      std::memcpy(start, r.to.data(), r.to.size());
      std::memmove(start + r.to.size(), tail, nul - tail);
      char* new_nul = start + r.to.size() + (nul - tail);
      std::memset(new_nul, 0, nul - new_nul);
      hit = start + r.to.size();
      done = true;
      ++changed;
      break;
    }
    if (!done) hit += kKey.size();
  }
  return changed;
}

}  // namespace

extern "C" REX_FUNC(sub_82B529A0) {
  const uint32_t strm = ctx.r3.u32;
  const uint32_t out_before = strm ? REX_LOAD_U32(strm + 12) : 0;  // z_stream.next_out
  __imp__sub_82B529A0(ctx, base);
  if (!strm || !REXCVAR_GET(wwe2k15_pc_texts)) return;
  const uint32_t out_after = REX_LOAD_U32(strm + 12);
  if (out_after <= out_before || out_after - out_before < kKey.size()) return;
  const int changed =
      Rewrite(reinterpret_cast<char*>(base + out_before), reinterpret_cast<char*>(base + out_after));
  static std::atomic<int> s_total{0};
  if (!changed) return;
  const int before = s_total.fetch_add(changed);
  if (before == 0 || before / 100 != (before + changed) / 100)
    REXLOG_INFO("Game texts: {} Xbox 360 mention(s) changed to PC so far", before + changed);
}
