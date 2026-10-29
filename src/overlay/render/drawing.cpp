#include "drawing.h"
#include <algorithm>
#include <cmath>

static ImColor with_alpha(ImColor color, float alpha) {
    ImVec4 v = color.Value;
    return ImColor(v.x, v.y, v.z, alpha);
}

ImColor drawing::outline_color = ImColor(0, 0, 0, 255);

void drawing::rect(float x, float y, float w, float h, ImColor color, float thickness, float outline) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 minimum = {x, y};
    ImVec2 maximum = {x + w, y + h};

    if (outline > 0.0f) {
        draw_list->AddRect(minimum, maximum, with_alpha(outline_color, color.Value.w), 0.0f, 0,
                           thickness + outline * 2.0f);
    }
    draw_list->AddRect(minimum, maximum, color, 0.0f, 0, thickness);
}

void drawing::line(ImVec2 from, ImVec2 to, ImColor color, float thickness, float outline) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (outline > 0.0f) {
        draw_list->AddLine(from, to, with_alpha(outline_color, color.Value.w), thickness + outline * 2.0f);
    }
    draw_list->AddLine(from, to, color, thickness);
}

void drawing::text(const char* text, float x, float y, ImColor color, float font_size, float outline,
                   ImFont* font) {
    ImFont* current_font = font ? font : ImGui::GetFont();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    float pos_x = std::round(x);
    float pos_y = std::round(y);

    if (outline > 0.0f) {
        ImColor outline_color_value = with_alpha(outline_color, color.Value.w);
        int outline_thickness = std::max(1, static_cast<int>(std::round(outline)));

        draw_list->AddText(current_font, font_size, {pos_x - outline_thickness, pos_y}, outline_color_value,
                           text);
        draw_list->AddText(current_font, font_size, {pos_x + outline_thickness, pos_y}, outline_color_value,
                           text);
        draw_list->AddText(current_font, font_size, {pos_x, pos_y - outline_thickness}, outline_color_value,
                           text);
        draw_list->AddText(current_font, font_size, {pos_x, pos_y + outline_thickness}, outline_color_value,
                           text);
    }

    draw_list->AddText(current_font, font_size, {pos_x, pos_y}, color, text);
}

void drawing::circle(float x, float y, float radius, ImColor color, float thickness, float outline) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (outline > 0.0f) {
        draw_list->AddCircle({x, y}, radius, with_alpha(outline_color, color.Value.w), 0,
                             thickness + outline * 2.0f);
    }
    draw_list->AddCircle({x, y}, radius, color, 0, thickness);
}

void drawing::filled_circle(float x, float y, float radius, ImColor color, float outline) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (outline > 0.0f) {
        draw_list->AddCircleFilled({x, y}, radius + outline, with_alpha(outline_color, color.Value.w), 0);
    }
    draw_list->AddCircleFilled({x, y}, radius, color, 0);
}

void drawing::filled_rect(float x, float y, float w, float h, ImColor color, float outline) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (outline > 0.0f) {
        draw_list->AddRectFilled({x - outline, y - outline}, {x + w + outline, y + h + outline},
                                 with_alpha(outline_color, color.Value.w));
    }
    draw_list->AddRectFilled({x, y}, {x + w, y + h}, color);
}