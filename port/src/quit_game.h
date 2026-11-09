// wwe2k15 - clean quit from the in-game settings menu
#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// Closes the game the way the window's close button does (WM_CLOSE to this process's windows), which shuts
// the game down cleanly; asking the UI loop to quit from a menu callback left the process hanging.
inline void Wwe2k15PostClose() {
#if defined(_WIN32)
  EnumWindows(
      [](HWND hwnd, LPARAM) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return TRUE;
      },
      0);
#endif
}
