#include "menu.h"
#include <imgui.h>

void Menu::render(float scale, bool visible, bool& capture_bypass) {
    if (!visible)
        return;
    if (scale <= 0.0f)
        scale = 1.0f;

    ImVec2 window_size = ImVec2(menu_width * scale, menu_height * scale);

    ImVec2 display = ImGui::GetIO().DisplaySize;
    if (last_scale != scale)
        ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((display.x - window_size.x) * 0.5f, (display.y - window_size.y) * 0.5f),
                            ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        ImVec2 current_size = ImGui::GetWindowSize();
        menu_width = current_size.x / scale;
        menu_height = current_size.y / scale;

        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Overlay size: %.0f x %.0f", display.x, display.y);
        ImGui::Checkbox("Capture bypass", &capture_bypass);
    }
    ImGui::End();
    last_scale = scale;
}
