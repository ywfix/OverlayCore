#pragma once

#include "../config.h"
#if OVERLAY_VIRTUAL_CURSOR
#include <windows.h>

class VirtualCursor {
public:
    ~VirtualCursor();
    VirtualCursor() = default;
    VirtualCursor(const VirtualCursor&) = delete;
    VirtualCursor& operator=(const VirtualCursor&) = delete;

    void initialize(HWND window);
    void update_bounds(int width, int height);
    void mark_rect_dirty() { rect_dirty = true; }
    bool is_active() const { return active; }
    void enable();
    void disable();
    void raw_input(LPARAM input);
    void update();

private:
    HWND window_handle = nullptr;
    int max_x = 0;
    int max_y = 0;
    float x = 0.0f;
    float y = 0.0f;
    bool active = false;
    bool rect_dirty = true;
    RECT previous_clip{};
    bool clip_saved = false;
    int hide_calls = 0;

    bool register_input(bool enable);
    bool clip_to_window();
};
#endif
