#pragma once

#include "config.h"
#include "menu.h"
#include "render/render.h"
#include <windows.h>
#if OVERLAY_VIRTUAL_CURSOR
#include "render/cursor.h"
#endif

class WindowManager {
public:
    WindowManager();
    ~WindowManager();
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    bool initialize();
#if OVERLAY_TARGET_MODE
    bool run(HWND target);
    bool is_target_focused() const;
#else
    bool run();
#endif
    void mark_dpi_dirty();
#if OVERLAY_TARGET_MODE || OVERLAY_VIRTUAL_CURSOR
    void focus_target();
#endif
    static WindowManager* get_instance();
#if OVERLAY_VIRTUAL_CURSOR
    VirtualCursor& get_cursor() { return cursor; }
#endif

private:
    static constexpr char class_name[] = "Overlay";
    static constexpr DWORD retry_delay_ms = 100;
    static WindowManager* instance;

    HWND window_handle = nullptr;
    Menu menu;
#if OVERLAY_VIRTUAL_CURSOR
    VirtualCursor cursor;
#endif
    float dpi_scale = 1.0f;
    bool running = false;
    bool dpi_dirty = false;
    bool toggle_pressed = false;
    bool visible = false;
    bool class_registered = false;
    bool render_failed = false;
    bool menu_visible = true;
    bool capture_bypass = false;
    bool applied_capture_bypass = false;
#if OVERLAY_USE_ZBID
    bool zbid_active = false;
#endif
#if OVERLAY_ZORDER_MODE
    bool owner_failed = false;
    void update_z_order();
#endif
#if OVERLAY_TARGET_MODE
    HWND target_handle = nullptr;
    DWORD target_pid = 0;
    RECT target_bounds{};
    bool is_target_valid() const;
    bool is_target_visible() const;
    void sync_target_bounds();
#else
    void sync_display_bounds();
#endif

    void handle_keyboard_input();
    void update_input(bool focused);
    void update_visibility(bool show);
    void update_capture_mode();
    void update_dpi_scale();
    void keep_on_top();
    void request_close();
    void render_frame();
    bool handle_frame_status(renderer::FrameStatus status);
    void shutdown();
};
