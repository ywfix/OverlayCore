#pragma once

class Menu {
public:
    Menu() = default;

    void render(float scale, bool visible, bool& capture_bypass);

private:
    float menu_width = 450.0f;
    float menu_height = 350.0f;
    float last_scale = 0.0f;
};