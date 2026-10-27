// Dev tool (no game rebuild needed): find a text in the running game's memory and see which code
// writes it. Two modes:
//   memtrap scan <pid> <text>          lists every copy of <text> (ASCII, UTF-16LE, UTF-16BE) as host
//                                      address + the region it sits in
//   memtrap trap <pid> <seconds> <hex host address>...
//                                      attaches as a debugger, sets PAGE_GUARD on the pages of those
//                                      addresses, and logs the RIP, the access (R/W), the address and the
//                                      return addresses found on the stack for every access, then detaches.
// Symbolize RIPs with: llvm-symbolizer --obj wwe2k15.exe 0x<rip - 0x140000000 + 0x140000000> (the exe is
// loaded at 0x140000000) and wwe2k15.pdb beside it.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

static void Scan(HANDLE process, const std::string& text) {
  std::vector<std::string> needles;
  needles.push_back(text);
  std::string le, be;
  for (char c : text) {
    le += c; le += '\0';
    be += '\0'; be += c;
  }
  needles.push_back(le);
  needles.push_back(be);
  const char* kinds[] = {"ascii", "utf16le", "utf16be"};
  MEMORY_BASIC_INFORMATION mbi;
  std::vector<char> buf;
  unsigned long long address = 0;
  int hits = 0;
  while (VirtualQueryEx(process, (LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
    const unsigned long long base = (unsigned long long)mbi.BaseAddress;
    if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) && mbi.RegionSize < (1ull << 33)) {
      for (unsigned long long off = 0; off < mbi.RegionSize; off += 64ull << 20) {
        const SIZE_T chunk = (SIZE_T)std::min<unsigned long long>(mbi.RegionSize - off, (64ull << 20) + 64);
        buf.resize(chunk);
        SIZE_T got = 0;
        if (!ReadProcessMemory(process, (LPCVOID)(base + off), buf.data(), chunk, &got) || !got) continue;
        for (size_t k = 0; k < needles.size(); ++k) {
          const std::string& n = needles[k];
          for (size_t i = 0; i + n.size() <= got; ++i) {
            if (buf[i] == n[0] && memcmp(&buf[i], n.data(), n.size()) == 0) {
              printf("%s host %p (region %p size %llx prot %lx type %lx)\n", kinds[k], (void*)(base + off + i),
                     mbi.BaseAddress, (unsigned long long)mbi.RegionSize, mbi.Protect, mbi.Type);
              if (++hits > 400) { printf("... stopped after 400 hits\n"); return; }
            }
          }
        }
      }
    }
    address = base + mbi.RegionSize;
    if (address == 0) break;
  }
  printf("%d hit(s)\n", hits);
}

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: memtrap scan <pid> <text> | trap <pid> <seconds> <hex>...\n"); return 2; }
  const std::string mode = argv[1];
  const DWORD pid = (DWORD)atoi(argv[2]);
  if (mode == "scan") {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) { fprintf(stderr, "OpenProcess failed %lu\n", GetLastError()); return 1; }
    Scan(process, argv[3]);
    return 0;
  }
  const int seconds = atoi(argv[3]);
  std::set<unsigned long long> pages;
  for (int i = 4; i < argc; ++i) pages.insert(strtoull(argv[i], nullptr, 16) & ~0xFFFull);
  if (!DebugActiveProcess(pid)) { fprintf(stderr, "DebugActiveProcess failed %lu\n", GetLastError()); return 1; }
  DebugSetProcessKillOnExit(FALSE);
  HANDLE process = nullptr;
  const ULONGLONG end = GetTickCount64() + (ULONGLONG)seconds * 1000;
  std::map<DWORD, unsigned long long> rearm;  // thread -> page to protect again after one step
  std::map<unsigned long long, int> counts;
  bool armed = false;
  bool stopping = false;
  unsigned long long img_base = 0, img_end = 0;
  ULONGLONG hard_end = end + 3000;
  while (GetTickCount64() < hard_end) {
    if (!stopping && GetTickCount64() >= end) stopping = true;
    if (stopping && rearm.empty()) break;
    DEBUG_EVENT ev;
    if (!WaitForDebugEvent(&ev, 200)) continue;
    DWORD status = DBG_CONTINUE;
    if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
      process = ev.u.CreateProcessInfo.hProcess;
      img_base = (unsigned long long)ev.u.CreateProcessInfo.lpBaseOfImage;
      {
        IMAGE_DOS_HEADER dos;
        IMAGE_NT_HEADERS64 nt;
        SIZE_T got = 0;
        if (ReadProcessMemory(process, (LPCVOID)img_base, &dos, sizeof(dos), &got) &&
            ReadProcessMemory(process, (LPCVOID)(img_base + dos.e_lfanew), &nt, sizeof(nt), &got))
          img_end = img_base + nt.OptionalHeader.SizeOfImage;
      }
      printf("image base %llx end %llx\n", img_base, img_end);
    } else if (ev.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT || ev.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT ||
               ev.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT || ev.dwDebugEventCode == UNLOAD_DLL_DEBUG_EVENT ||
               ev.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT) {
      // nothing
    } else if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
      const EXCEPTION_RECORD& er = ev.u.Exception.ExceptionRecord;
      if (er.ExceptionCode == EXCEPTION_GUARD_PAGE) {
        const unsigned long long target = er.ExceptionInformation[1];
        const unsigned long long page = target & ~0xFFFull;
        if (pages.count(page)) {
          HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, ev.dwThreadId);
          CONTEXT ctx = {};
          ctx.ContextFlags = CONTEXT_FULL;
          if (thread && GetThreadContext(thread, &ctx)) {
            const unsigned long long key = ctx.Rip * 2 + (er.ExceptionInformation[0] ? 1 : 0);
            ++counts[key];
            if (counts[key] <= 3 || er.ExceptionInformation[0]) {
              printf("%s t%lu rip %llx addr %llx", er.ExceptionInformation[0] ? "W" : "R", ev.dwThreadId,
                     (unsigned long long)ctx.Rip, target);
              unsigned long long stack[256];
              SIZE_T got = 0;
              if (ReadProcessMemory(process, (LPCVOID)ctx.Rsp, stack, sizeof(stack), &got)) {
                int shown = 0;
                for (size_t i = 0; i < got / 8 && shown < 6; ++i) {
                  if (stack[i] >= img_base && stack[i] < img_end) {
                    printf(" ret %llx", stack[i]);
                    ++shown;
                  }
                }
              }
              printf("\n");
              fflush(stdout);
            }
            if (!stopping) {
              ctx.EFlags |= 0x100;  // single step, then protect the page again
              SetThreadContext(thread, &ctx);
              rearm[ev.dwThreadId] = page;
            }
          }
          if (thread) CloseHandle(thread);
        } else {
          status = DBG_EXCEPTION_NOT_HANDLED;
        }
      } else if (er.ExceptionCode == EXCEPTION_SINGLE_STEP) {
        auto it = rearm.find(ev.dwThreadId);
        if (it != rearm.end()) {
          DWORD old;
          if (!stopping) VirtualProtectEx(process, (LPVOID)it->second, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
          rearm.erase(it);
        } else {
          status = DBG_EXCEPTION_NOT_HANDLED;
        }
      } else if (er.ExceptionCode == EXCEPTION_BREAKPOINT && !armed && process) {
        // The initial breakpoint of the attach: arm the pages now.
        for (unsigned long long page : pages) {
          MEMORY_BASIC_INFORMATION mbi;
          if (VirtualQueryEx(process, (LPCVOID)page, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            DWORD old;
            if (!VirtualProtectEx(process, (LPVOID)page, 0x1000, (mbi.Protect & 0xFF) | PAGE_GUARD, &old))
              fprintf(stderr, "protect %llx failed %lu\n", page, GetLastError());
            else
              printf("armed page %llx (was prot %lx)\n", page, mbi.Protect);
          }
        }
        armed = true;
      } else {
        status = DBG_EXCEPTION_NOT_HANDLED;
      }
    } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
      break;
    }
    ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
  }
  // un-guard the pages again before letting go
  for (unsigned long long page : pages) {
    MEMORY_BASIC_INFORMATION mbi;
    if (process && VirtualQueryEx(process, (LPCVOID)page, &mbi, sizeof(mbi)) == sizeof(mbi) && (mbi.Protect & PAGE_GUARD)) {
      DWORD old;
      VirtualProtectEx(process, (LPVOID)page, 0x1000, mbi.Protect & ~PAGE_GUARD, &old);
    }
  }
  DebugActiveProcessStop(pid);
  printf("accesses per rip:\n");
  for (auto& kv : counts) printf("  %llx x%d\n", kv.first, kv.second);
  return 0;
}
