#pragma once
#include <ShellScalingApi.h>
#include <Windows.h>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT) - 4)
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

class DpiHelper {
public:
    static UINT get_window_dpi(HWND hwnd) {
        static auto get_dpi_for_window = reinterpret_cast<UINT(WINAPI*)(HWND)>(
            GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "GetDpiForWindow"));
        if (get_dpi_for_window && hwnd) {
            const UINT dpi = get_dpi_for_window(hwnd);
            if (dpi)
                return dpi;
        }

        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return 96;
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
        return dpi > 0 ? static_cast<UINT>(dpi) : 96;
    }

    static bool initialize() {
        static auto set_awareness_context = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
            GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "SetProcessDpiAwarenessContext"));
        if (set_awareness_context && set_awareness_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            return true;

        const HMODULE shcore = LoadLibraryW(L"Shcore.dll");
        if (shcore) {
            const auto set_awareness = reinterpret_cast<HRESULT(WINAPI*)(PROCESS_DPI_AWARENESS)>(
                GetProcAddress(shcore, "SetProcessDpiAwareness"));
            const HRESULT result = set_awareness ? set_awareness(PROCESS_PER_MONITOR_DPI_AWARE) : E_NOTIMPL;
            FreeLibrary(shcore);
            if (SUCCEEDED(result))
                return true;
        }

        return ::SetProcessDPIAware();
    }

    static float get_scale(HWND hwnd) { return static_cast<float>(get_window_dpi(hwnd)) / 96.0f; }
};