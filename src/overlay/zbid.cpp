#include "zbid.h"
#if OVERLAY_USE_ZBID

bool zbid::apply(HWND window) {
    if (!IsWindow(window))
        return false;
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return false;
    using SetWindowBand = BOOL(WINAPI*)(HWND, HWND, DWORD);
    const auto set_window_band = reinterpret_cast<SetWindowBand>(GetProcAddress(user32, "SetWindowBand"));
    return set_window_band && set_window_band(window, nullptr, 2) != FALSE;
}
#endif
