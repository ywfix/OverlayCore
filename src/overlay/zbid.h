#pragma once

#include "config.h"
#if OVERLAY_USE_ZBID
#include <windows.h>

namespace zbid {
bool apply(HWND window);
}
#endif
