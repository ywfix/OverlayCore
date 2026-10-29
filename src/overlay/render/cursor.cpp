#include "cursor.h"
#if OVERLAY_VIRTUAL_CURSOR
#include <algorithm>
#include <imgui.h>

VirtualCursor::~VirtualCursor() {
    disable();
}

void VirtualCursor::initialize(HWND window) {
    window_handle = window;
    RECT bounds{};
    if (GetClientRect(window, &bounds))
        update_bounds(bounds.right, bounds.bottom);
}

void VirtualCursor::update_bounds(int width, int height) {
    max_x = std::max(0, width - 1);
    max_y = std::max(0, height - 1);
    x = std::clamp(x, 0.0f, static_cast<float>(max_x));
    y = std::clamp(y, 0.0f, static_cast<float>(max_y));
    rect_dirty = true;
    if (active && ImGui::GetCurrentContext())
        ImGui::GetIO().AddMousePosEvent(x, y);
}

bool VirtualCursor::register_input(bool enable) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
    device.hwndTarget = enable ? window_handle : nullptr;
    return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

bool VirtualCursor::clip_to_window() {
    RECT bounds{};
    return GetWindowRect(window_handle, &bounds) && ClipCursor(&bounds);
}

void VirtualCursor::enable() {
    if (active || !IsWindow(window_handle) || !ImGui::GetCurrentContext() ||
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        return;
    clip_saved = GetClipCursor(&previous_clip) != FALSE;
    POINT position{};
    if (GetCursorPos(&position) && ScreenToClient(window_handle, &position)) {
        x = std::clamp(static_cast<float>(position.x), 0.0f, static_cast<float>(max_x));
        y = std::clamp(static_cast<float>(position.y), 0.0f, static_cast<float>(max_y));
    } else {
        x = max_x / 2.0f;
        y = max_y / 2.0f;
    }
    if (!register_input(true))
        return;
    if (!clip_to_window()) {
        register_input(false);
        return;
    }
    active = true;
    rect_dirty = false;
    ReleaseCapture();
    hide_calls = 0;
    for (int i = 0; i < 32; ++i) {
        ++hide_calls;
        if (ShowCursor(FALSE) < 0)
            break;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = true;
    io.AddMousePosEvent(x, y);
}

void VirtualCursor::disable() {
    if (!active)
        return;
    active = false;
    register_input(false);
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearEventsQueue();
        io.ClearInputMouse();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        io.MouseDrawCursor = false;
    }
    POINT position{static_cast<LONG>(x), static_cast<LONG>(y)};
    if (IsWindow(window_handle) && ClientToScreen(window_handle, &position))
        SetCursorPos(position.x, position.y);
    const HWND foreground = GetForegroundWindow();
    const bool returning =
        foreground && (foreground == window_handle || foreground == GetWindow(window_handle, GW_OWNER));
    ClipCursor(clip_saved && returning ? &previous_clip : nullptr);
    for (int i = 0; i < hide_calls; ++i)
        ShowCursor(TRUE);
    hide_calls = 0;
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
}

void VirtualCursor::raw_input(LPARAM input) {
    if (!active)
        return;
    RAWINPUT packet{};
    UINT size = sizeof(packet);
    const UINT received = GetRawInputData(reinterpret_cast<HRAWINPUT>(input), RID_INPUT, &packet, &size,
                                          sizeof(RAWINPUTHEADER));
    if (received == static_cast<UINT>(-1) || received < sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) ||
        received > sizeof(packet) || packet.header.dwType != RIM_TYPEMOUSE)
        return;
    if (packet.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
        const bool desktop = (packet.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
        const int left = desktop ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0;
        const int top = desktop ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
        const int width = GetSystemMetrics(desktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
        const int height = GetSystemMetrics(desktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
        POINT position{left + MulDiv(packet.data.mouse.lLastX, width - 1, 65535),
                       top + MulDiv(packet.data.mouse.lLastY, height - 1, 65535)};
        if (!ScreenToClient(window_handle, &position))
            return;
        x = static_cast<float>(position.x);
        y = static_cast<float>(position.y);
    } else {
        x += packet.data.mouse.lLastX;
        y += packet.data.mouse.lLastY;
    }
    x = std::clamp(x, 0.0f, static_cast<float>(max_x));
    y = std::clamp(y, 0.0f, static_cast<float>(max_y));
    ImGui::GetIO().AddMousePosEvent(x, y);
}

void VirtualCursor::update() {
    if (active && rect_dirty && clip_to_window())
        rect_dirty = false;
}
#endif
