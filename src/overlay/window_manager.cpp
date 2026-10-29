#include "window_manager.h"
#include "background.h"
#include "render/dpi_utils.h"
#if OVERLAY_USE_ZBID
#include "zbid.h"
#endif
#include <algorithm>
#include <dwmapi.h>

WindowManager* WindowManager::instance = nullptr;

WindowManager::WindowManager() {
    instance = this;
    DpiHelper::initialize();
}

WindowManager::~WindowManager() {
    shutdown();
    if (instance == this)
        instance = nullptr;
}

WindowManager* WindowManager::get_instance() {
    return instance;
}

bool WindowManager::initialize() {
    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = renderer::window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = class_name;
    if (!RegisterClassExA(&window_class))
        return false;
    class_registered = true;

#if OVERLAY_TARGET_MODE
    const int width = 1;
    const int height = 1;
#else
    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);
#endif
    constexpr DWORD style = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    window_handle = CreateWindowExA(style, class_name, "OverlayWindow", WS_POPUP, 0, 0, width, height,
                                    nullptr, nullptr, window_class.hInstance, nullptr);
    if (!window_handle)
        return false;

#if OVERLAY_USE_ZBID
    zbid_active = zbid::apply(window_handle);
#endif
    const MARGINS margins{-1, -1, -1, -1};
    if (FAILED(DwmExtendFrameIntoClientArea(window_handle, &margins)))
        return false;
    if (!SetLayeredWindowAttributes(window_handle, 0, 255, LWA_ALPHA))
        return false;
    dpi_scale = DpiHelper::get_scale(window_handle);
    if (!renderer::initialize(window_handle, dpi_scale))
        return false;
#if OVERLAY_VIRTUAL_CURSOR
    cursor.initialize(window_handle);
#endif
    return true;
}

#if OVERLAY_TARGET_MODE
bool WindowManager::is_target_valid() const {
    DWORD current_pid = 0;
    return target_handle && target_pid && GetWindowThreadProcessId(target_handle, &current_pid) &&
           current_pid == target_pid;
}

bool WindowManager::is_target_visible() const {
    const HWND root = GetAncestor(target_handle, GA_ROOT);
    return root && !IsIconic(root) && IsWindowVisible(target_handle);
}

bool WindowManager::is_target_focused() const {
    if (!is_target_valid() || !is_target_visible())
        return false;
    const HWND foreground = GetForegroundWindow();
    return foreground && (foreground == window_handle || foreground == target_handle ||
                          foreground == GetAncestor(target_handle, GA_ROOT));
}

void WindowManager::sync_target_bounds() {
    RECT bounds{};
    POINT origin{};
    if (!GetClientRect(target_handle, &bounds) || !ClientToScreen(target_handle, &origin))
        return;
    bounds.right = origin.x + std::max(1L, bounds.right - bounds.left);
    bounds.bottom = origin.y + std::max(1L, bounds.bottom - bounds.top);
    bounds.left = origin.x;
    bounds.top = origin.y;
    if (EqualRect(&bounds, &target_bounds))
        return;
    if (SetWindowPos(window_handle, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, SWP_NOACTIVATE | SWP_NOZORDER)) {
        target_bounds = bounds;
    }
}

bool WindowManager::run(HWND target) {
    target_handle = target;
    GetWindowThreadProcessId(target_handle, &target_pid);
    if (!is_target_valid())
        return false;
    const HWND root = GetAncestor(target_handle, GA_ROOT);
    if (IsIconic(root))
        ShowWindow(root, SW_RESTORE);
    if (menu_visible)
        SetForegroundWindow(root);
#else
bool WindowManager::run() {
#endif
    running = true;
    while (running) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running)
            break;
        update_dpi_scale();
        if (!running)
            break;

#if OVERLAY_TARGET_MODE
        if (!is_target_valid())
            break;
        const bool target_visible = is_target_visible();
        if (target_visible)
            sync_target_bounds();
        const bool focused = target_visible && is_target_focused();
#if OVERLAY_ZORDER_MODE
        update_visibility(target_visible);
#else
        update_visibility(target_visible && focused);
#endif
#else
        const bool focused = true;
        update_visibility(true);
#endif
        if (focused)
            handle_keyboard_input();
        else
            toggle_pressed = false;
        if (!running)
            break;

        update_input(focused);
        keep_on_top();
        if (!visible) {
            MsgWaitForMultipleObjects(0, nullptr, FALSE, retry_delay_ms, QS_ALLINPUT);
            continue;
        }
        render_frame();
    }
    shutdown();
    return !render_failed;
}

void WindowManager::update_visibility(bool show) {
    if (visible == show)
        return;
    if (show) {
        visible = true;
        keep_on_top();
    }
    ShowWindow(window_handle, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    visible = show;
}

void WindowManager::handle_keyboard_input() {
    if (GetAsyncKeyState(VK_END) & 0x8000) {
        request_close();
        return;
    }
    const bool pressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) || (GetAsyncKeyState(VK_HOME) & 0x8000);
    if (pressed && !toggle_pressed) {
        menu_visible = !menu_visible;
#if OVERLAY_TARGET_MODE
        focus_target();
#endif
    }
    toggle_pressed = pressed;
}

void WindowManager::update_input(bool focused) {
    const bool interactive = menu_visible && focused;
    const LONG_PTR style = GetWindowLongPtrW(window_handle, GWL_EXSTYLE);
    const LONG_PTR requested = interactive ? style & ~WS_EX_TRANSPARENT : style | WS_EX_TRANSPARENT;
    if (requested != style) {
        SetWindowLongPtrW(window_handle, GWL_EXSTYLE, requested);
        SetWindowPos(window_handle, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
#if OVERLAY_VIRTUAL_CURSOR
    if (interactive)
        cursor.enable();
    else
        cursor.disable();
#endif
}

#if OVERLAY_TARGET_MODE || OVERLAY_VIRTUAL_CURSOR
void WindowManager::focus_target() {
#if OVERLAY_TARGET_MODE
    if (is_target_valid())
        SetForegroundWindow(GetAncestor(target_handle, GA_ROOT));
#else
    SetForegroundWindow(window_handle);
#endif
}

#endif

void WindowManager::request_close() {
#if OVERLAY_VIRTUAL_CURSOR
    cursor.disable();
#endif
    running = false;
}

bool WindowManager::handle_frame_status(renderer::FrameStatus status) {
    switch (status) {
    case renderer::FrameStatus::Ready:
        return true;
    case renderer::FrameStatus::Fatal:
        render_failed = true;
        request_close();
        break;
    case renderer::FrameStatus::Occluded:
        renderer::wait_for_vsync();
        break;
    case renderer::FrameStatus::Retry:
        MsgWaitForMultipleObjects(0, nullptr, FALSE, retry_delay_ms, QS_ALLINPUT);
        break;
    }
    return false;
}

void WindowManager::render_frame() {
#if OVERLAY_VIRTUAL_CURSOR
    cursor.update();
#endif
    if (!handle_frame_status(renderer::begin_frame()))
        return;
    background::render(dpi_scale);
    menu.render(dpi_scale, menu_visible, capture_bypass);
    if (capture_bypass != applied_capture_bypass)
        update_capture_mode();
    handle_frame_status(renderer::present());
}

void WindowManager::update_capture_mode() {
    if (SetWindowDisplayAffinity(window_handle, capture_bypass ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE)) {
        applied_capture_bypass = capture_bypass;
    } else {
        capture_bypass = applied_capture_bypass;
        OutputDebugStringA("Overlay: display affinity update failed.\n");
    }
}

void WindowManager::update_dpi_scale() {
    if (!dpi_dirty)
        return;
    const float scale = DpiHelper::get_scale(window_handle);
    if (dpi_scale != scale) {
        dpi_scale = scale;
        if (!renderer::set_scale(scale)) {
            render_failed = true;
            request_close();
        }
    }
    dpi_dirty = false;
#if !OVERLAY_TARGET_MODE
    sync_display_bounds();
#endif
}

void WindowManager::mark_dpi_dirty() {
    dpi_dirty = true;
#if OVERLAY_TARGET_MODE
    target_bounds = {};
#endif
}

#if !OVERLAY_TARGET_MODE
void WindowManager::sync_display_bounds() {
    RECT bounds{};
    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);
    if (GetClientRect(window_handle, &bounds) && (bounds.right != width || bounds.bottom != height)) {
        SetWindowPos(window_handle, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
    }
}
#endif

#if OVERLAY_ZORDER_MODE
void WindowManager::update_z_order() {
    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    const HWND root = GetAncestor(target_handle, GA_ROOT);
    if (!root || IsIconic(root))
        return;
    if (!owner_failed && GetWindow(window_handle, GW_OWNER) != root) {
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR owner =
            SetWindowLongPtrW(window_handle, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(root));
        if (!owner && GetLastError() != ERROR_SUCCESS) {
            owner_failed = true;
            OutputDebugStringA("Overlay: target ownership unavailable\n");
        }
    }
    const bool target_topmost = (GetWindowLongPtrW(root, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    const bool overlay_topmost = (GetWindowLongPtrW(window_handle, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (target_topmost != overlay_topmost) {
        SetWindowPos(window_handle, target_topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    }
    HWND previous = GetWindow(root, GW_HWNDPREV);
    if (previous == window_handle)
        return;
    if (!previous || (!target_topmost && (GetWindowLongPtrW(previous, GWL_EXSTYLE) & WS_EX_TOPMOST))) {
        previous = HWND_TOP;
    }
    SetWindowPos(window_handle, previous, 0, 0, 0, 0, flags);
}
#endif

void WindowManager::keep_on_top() {
    if (!visible)
        return;
#if OVERLAY_ZORDER_MODE
    update_z_order();
#else
#if OVERLAY_USE_ZBID
    if (zbid_active)
        return;
#endif
    HWND previous = GetWindow(window_handle, GW_HWNDPREV);
    if (!(GetWindowLongPtrW(window_handle, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
        SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return;
    }
#if OVERLAY_TARGET_MODE
    const HWND root = GetAncestor(target_handle, GA_ROOT);
    while (previous && previous != root)
        previous = GetWindow(previous, GW_HWNDPREV);
#endif
    if (previous) {
        SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#endif
}

void WindowManager::shutdown() {
    running = false;
#if OVERLAY_VIRTUAL_CURSOR
    cursor.disable();
#endif
    renderer::shutdown();
    if (window_handle) {
        if (IsWindow(window_handle))
            DestroyWindow(window_handle);
        window_handle = nullptr;
    }
    if (class_registered) {
        UnregisterClassA(class_name, GetModuleHandleW(nullptr));
        class_registered = false;
    }
}
