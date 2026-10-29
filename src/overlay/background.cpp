#include "background.h"
#include "render/drawing.h"
#include <cmath>
#include <cstdio>

void background::render(float scale) {
    char text[32];
    sprintf_s(text, "FPS: %.1f", ImGui::GetIO().Framerate);
    drawing::text(text, 5, 3, ImColor(255, 255, 255), std::floor(14.0f * scale), 1.0f);
}
