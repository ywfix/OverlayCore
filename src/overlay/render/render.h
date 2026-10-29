#pragma once

#include <windows.h>

namespace renderer {
enum class FrameStatus { Ready, Occluded, Retry, Fatal };

bool initialize(HWND window, float dpi_scale);
void shutdown();
bool set_scale(float scale);
FrameStatus begin_frame();
FrameStatus present();
void wait_for_vsync();
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
}
