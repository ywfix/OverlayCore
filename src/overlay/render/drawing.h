#pragma once
#include <imgui.h>

namespace drawing {
extern ImColor outline_color;
void rect(float x, float y, float w, float h, ImColor color, float thickness = 1.0f, float outline = 0.0f);
void line(ImVec2 from, ImVec2 to, ImColor color, float thickness = 1.0f, float outline = 0.0f);
void text(const char* text, float x, float y, ImColor color, float font_size = 14.0f, float outline = 0.0f,
          ImFont* font = nullptr);
void circle(float x, float y, float radius, ImColor color, float thickness = 1.0f, float outline = 0.0f);
void filled_circle(float x, float y, float radius, ImColor color, float outline = 0.0f);
void filled_rect(float x, float y, float w, float h, ImColor color, float outline = 0.0f);
}