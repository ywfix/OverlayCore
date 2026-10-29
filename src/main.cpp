#include "overlay/window_manager.h"
#include <exception>

namespace {
struct Handle {
    explicit Handle(HANDLE handle) : value(handle) {}
    ~Handle() {
        if (value)
            CloseHandle(value);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE value;
};
}

#if OVERLAY_ENABLE_CONSOLE
#include <cstdio>

static void enable_console() {
    if (!AllocConsole()) return;
    SetConsoleOutputCP(CP_UTF8);
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONIN$", "r", stdin);
}
#endif

#if OVERLAY_ENABLE_CONSOLE && OVERLAY_TARGET_MODE
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
struct WindowInfo {
    HWND handle;
    std::string label;
};

std::string to_utf8(const std::wstring& text) {
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr,
                        nullptr);
    return result;
}

std::string get_title(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0)
        return {};
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    text.resize(copied > 0 ? static_cast<size_t>(copied) : 0);
    return to_utf8(text);
}

std::string get_process_name(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.value) return "Unknown";
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.value, 0, path.data(), &size)) return "Unknown";
    path.resize(size);
    const auto separator = path.find_last_of(L"\\");
    return to_utf8(separator == std::wstring::npos ? path : path.substr(separator + 1));
}

std::vector<WindowInfo> get_windows(HWND parent = nullptr) {
    struct Enumeration {
        std::vector<WindowInfo> windows;
        HWND parent;
    } enumeration{{}, parent};
    const auto callback = [](HWND window, LPARAM parameter) -> BOOL {
        auto& data = *reinterpret_cast<Enumeration*>(parameter);
        if (!IsWindowVisible(window))
            return TRUE;
        std::string label = get_title(window);
        if (data.parent) {
            wchar_t class_name[256]{};
            GetClassNameW(window, class_name, 256);
            label += " (" + to_utf8(class_name) + ")";
        } else {
            if (GetWindow(window, GW_OWNER) || window == GetConsoleWindow() || label.empty())
                return TRUE;
            label = get_process_name(window) + " (" + label + ")";
        }
        data.windows.push_back({window, std::move(label)});
        return TRUE;
    };
    const LPARAM parameter = reinterpret_cast<LPARAM>(&enumeration);
    if (parent)
        EnumChildWindows(parent, callback, parameter);
    else
        EnumWindows(callback, parameter);
    return enumeration.windows;
}

HWND select_window(const std::vector<WindowInfo>& windows, HWND fallback = nullptr) {
    const size_t offset = fallback ? 1 : 0;
    if (fallback) std::cout << "0) Main window\n";
    for (size_t i = 0; i < windows.size(); ++i) {
        std::cout << i + offset << ") " << windows[i].label << '\n';
    }
    std::cout << "\nSelect " << (fallback ? "child" : "main") << " window: ";
    size_t choice = 0;
    if (std::cin >> choice) {
        if (fallback && choice == 0)
            return fallback;
        if (choice >= offset && choice - offset < windows.size())
            return windows[choice - offset].handle;
    }
    std::cout << "Invalid selection.\n";
    return nullptr;
}

HWND pick_target_window() {
    const auto windows = get_windows();
    if (windows.empty()) {
        std::cout << "No visible windows found.\n";
        return nullptr;
    }
    const HWND target = select_window(windows);
    if (!target) return nullptr;
    const auto children = get_windows(target);
    return children.empty() ? target : select_window(children, target);
}
}
#endif

static int run_application() {
#if OVERLAY_TARGET_MODE && !OVERLAY_ENABLE_CONSOLE
    const HWND target = GetForegroundWindow();
#endif
#if OVERLAY_ENABLE_CONSOLE
    enable_console();
#endif
    Handle mutex(CreateMutexW(nullptr, FALSE, L"overlay-main.singleton"));
    if (!mutex.value || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Overlay is already running or its lock is unavailable.", L"Overlay", MB_ICONERROR | MB_OK);
        return 1;
    }
#if OVERLAY_TARGET_MODE
#if OVERLAY_ENABLE_CONSOLE
    const HWND target = pick_target_window();
#endif
    if (!target || !IsWindow(target))
        return 1;
#endif
    WindowManager manager;
    if (!manager.initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize the overlay.", L"Overlay", MB_ICONERROR | MB_OK);
        return 1;
    }
#if OVERLAY_TARGET_MODE
    return manager.run(target) ? 0 : 1;
#else
    return manager.run() ? 0 : 1;
#endif
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        return run_application();
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        MessageBoxW(nullptr, L"The overlay encountered an unexpected error.", L"Overlay", MB_ICONERROR | MB_OK);
        return 1;
    }
}
