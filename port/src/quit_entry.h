// wwe2k15 - main menu SHOP -> EXIT (quit_entry.cpp): the app registers how to ask the player to confirm quitting
#pragma once

#include <functional>

inline std::function<void()>& Wwe2k15QuitConfirmRequest() {
  static std::function<void()> f;
  return f;
}
