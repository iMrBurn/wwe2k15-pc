// wwe2k15 - ReXGlue Recompiled Project

#if defined(_WIN32)
// Ask NVIDIA Optimus to expose and prefer the discrete GPU for this title.
// Vulkan still selects the physical device through ReXGlue's normal provider;
// this export makes the high-performance adapter available to that selection.
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
#endif

#include "generated/default/wwe2k15_init.h"

#include "wwe2k15_app.h"

REX_DEFINE_APP(wwe2k15, Wwe2k15App::Create)
