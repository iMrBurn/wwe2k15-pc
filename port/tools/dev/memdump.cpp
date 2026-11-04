// Dev tool (no game rebuild needed): snapshot ranges of the running game's guest memory, to diff two
// snapshots (for example the Options menu with an option ON and OFF) with memdiff.py.
//   memdump base <pid>                          finds where guest memory starts in the host process (checks the
//                                               frame-rate struct at guest 0x838DC7A8, +8 = fps 25/30/50/60)
//   memdump dump <pid> <out.bin> <guest lo hex> <guest hi hex> [host base hex]
//                                               writes the committed pages of [lo, hi) as records
//                                               {u32 guest addr, u32 size, bytes} (big-endian guest bytes
//                                               exactly as in memory); skips unreadable pages
// Reads only (PROCESS_VM_READ); the game is not stopped, so take both snapshots while the game sits still on the
// same menu screen. Build: clang-cl /EHsc /O2 memdump.cpp (or the same line as memtrap.cpp).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static bool Readable(const MEMORY_BASIC_INFORMATION& m) {
  return m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}

// Candidate guest bases: starts of host regions (committed or reserved) that begin a >= 1 GB run.
static unsigned long long FindBase(HANDLE process, bool verbose) {
  MEMORY_BASIC_INFORMATION mbi;
  unsigned long long address = 0;
  std::vector<unsigned long long> cand;
  while (VirtualQueryEx(process, (LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
    const unsigned long long base = (unsigned long long)mbi.BaseAddress;
    if (mbi.RegionSize >= (1ull << 26) && mbi.State != MEM_FREE) cand.push_back(base);
    address = base + mbi.RegionSize;
    if (!address) break;
  }
  // The guest block can be split in many host regions; also try every region start minus the 0x82xxxxxx offsets.
  unsigned long long found = 0;
  for (unsigned long long b : cand) {
    for (unsigned long long off : {0ull, 0x100000000ull, 0x80000000ull, 0x40000000ull}) {
      if (b < off) continue;
      const unsigned long long g = b - off;
      uint8_t w[4];
      SIZE_T got = 0;
      if (!ReadProcessMemory(process, (LPCVOID)(g + 0x838DC7A8 + 8), w, 4, &got) || got != 4) continue;
      const uint32_t fps = Be32(w);
      if (fps == 25 || fps == 30 || fps == 50 || fps == 60) {
        if (verbose) printf("candidate guest base %llx (fps %u)\n", g, fps);
        if (!found) found = g;
      }
    }
  }
  return found;
}

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: memdump base <pid> | dump <pid> <out> <lo> <hi> [host base]\n"); return 2; }
  const std::string mode = argv[1];
  const DWORD pid = (DWORD)atoi(argv[2]);
  HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!process) { fprintf(stderr, "OpenProcess failed %lu\n", GetLastError()); return 1; }
  if (mode == "base") {
    const unsigned long long b = FindBase(process, true);
    printf(b ? "guest base %llx\n" : "no base found (is the game past the first screens?) %llx\n", b);
    return b ? 0 : 1;
  }
  if (mode != "dump" || argc < 6) { fprintf(stderr, "bad arguments\n"); return 2; }
  const unsigned long long lo = strtoull(argv[4], nullptr, 16), hi = strtoull(argv[5], nullptr, 16);
  unsigned long long base = argc > 6 ? strtoull(argv[6], nullptr, 16) : FindBase(process, false);
  if (!base) { fprintf(stderr, "guest base not found\n"); return 1; }
  FILE* out = fopen(argv[3], "wb");
  if (!out) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
  std::vector<uint8_t> buf;
  unsigned long long total = 0;
  unsigned long long g = lo;
  while (g < hi) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(process, (LPCVOID)(base + g), &mbi, sizeof(mbi)) != sizeof(mbi)) break;
    const unsigned long long rbase = (unsigned long long)mbi.BaseAddress;
    const unsigned long long rend = std::min<unsigned long long>(rbase + mbi.RegionSize, base + hi);
    const unsigned long long start = std::max<unsigned long long>(rbase, base + g);
    if (Readable(mbi) && rend > start) {
      for (unsigned long long a = start; a < rend; a += 1u << 24) {
        const size_t n = (size_t)std::min<unsigned long long>(rend - a, 1u << 24);
        buf.resize(n);
        SIZE_T got = 0;
        if (!ReadProcessMemory(process, (LPCVOID)a, buf.data(), n, &got) || !got) continue;
        const uint32_t ga = uint32_t(a - base), sz = uint32_t(got);
        fwrite(&ga, 4, 1, out);
        fwrite(&sz, 4, 1, out);
        fwrite(buf.data(), 1, got, out);
        total += got;
      }
    }
    g = (rbase + mbi.RegionSize) - base;
  }
  fclose(out);
  printf("host base %llx, wrote %llu bytes from guest %llx to %llx\n", base, total, lo, hi);
  return 0;
}
