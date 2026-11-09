#include "nx/runtime/platform/input_prompts.h"
#include "gui_theme_colors.h"
#include <stdio.h>
#include <string.h>

int g_input_prompts_mode = 1;
static Ykw4InputFamily active_family;
static SnxPadMap active_map;
static int discord_enabled = -1;

static int control_for(const SnxPadMap* map, int button) {
    if (!map || button < 0 || button >= SNX_PAD_BUTTONS) return -1;
    for (int control = 0; control < SNX_PAD_CONTROLS; ++control)
        if (map->bit[control] == button) return control;
    return -1;
}

const char* input_prompt_label(Ykw4InputFamily family, const SnxPadMap* map, int button) {
    static const char* const keys[SNX_PAD_BUTTONS] = {
        "X/Spc", "Z", "C", "V", "--", "--", "Q", "E", "1", "3", "Enter", "Bksp", "Left", "Up", "Right", "Down"};
    static const char* const faces[][4] = {
        {"", "", "", ""}, {"A", "B", "X", "Y"}, {"Cross", "Circle", "Square", "Tri"},
        {"B", "A", "Y", "X"}, {"South", "East", "West", "North"}};
    static const char* const generic[SNX_PAD_CONTROLS] = {
        "South", "East", "West", "North", "LB", "RB", "LT", "RT", "LS", "RS", "Back", "Start", "Up", "Down", "Left", "Right"};
    static const char* const playstation[SNX_PAD_CONTROLS] = {
        "Cross", "Circle", "Square", "Tri", "L1", "R1", "L2", "R2", "L3", "R3", "Share", "Opts", "Up", "Down", "Left", "Right"};
    static const char* const nintendo[SNX_PAD_CONTROLS] = {
        "B", "A", "Y", "X", "L", "R", "ZL", "ZR", "LS", "RS", "-", "+", "Up", "Down", "Left", "Right"};
    if (button < 0 || button >= SNX_PAD_BUTTONS) return "--";
    if (family == YKW4_INPUT_KEYBOARD) return keys[button];
    int control = control_for(map, button);
    if (control < 0) return "--";
    if (family < YKW4_INPUT_XBOX || family > YKW4_INPUT_GENERIC) family = YKW4_INPUT_GENERIC;
    if (control < 4) return faces[family][control];
    if (family == YKW4_INPUT_PLAYSTATION) return playstation[control];
    if (family == YKW4_INPUT_SWITCH) return nintendo[control];
    return generic[control];
}

void input_prompts_update(Ykw4InputFamily family, const SnxPadMap* map, int discord) {
    active_family = family;
    if (map) active_map = *map;
    else memset(&active_map, SNX_PAD_NONE, sizeof active_map);
    discord_enabled = discord;
}

static u32 rgba(u32 rgb, u32 alpha) {
    return alpha << 24 | (rgb & 255) << 16 | (rgb & 0xff00) | (rgb >> 16 & 255);
}

static void face_symbol(SnxUiList* list, int control, float left, float top, u32 color) {
    static const unsigned char shapes[4][7] = {
        {0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41},
        {0x1c, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1c},
        {0x7f, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7f},
        {0x08, 0x14, 0x14, 0x22, 0x22, 0x41, 0x7f}};
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 7; ++column)
            if (shapes[control][row] & (1 << column))
                ui_fill(list, left + column * 2, top + row * 2, 2, 2, color);
}

void input_prompts_draw(SnxUiList* list, const int safe[4]) {
    static const int buttons[] = {0, 1, 2, 3, 6, 7, 8, 9, 10, 11};
    static const char* const names[] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "+", "-"};
    if (!g_input_prompts_mode || safe[2] < 208 || safe[3] < 140) return;
    int columns = (safe[2] - 16) / 96;
    if (columns > 10) columns = 10;
    int rows = (10 + columns - 1) / columns;
    int width = columns * 96 + 16, height = rows * 26 + 46;
    if (height > safe[3] - 8) return;
    float left = safe[0] + (safe[2] - width) / 2.0f;
    float top = safe[1] + safe[3] - height - 4;
    u32 panel = rgba(SNX_GUI_THEME_YORHA_INK, 232);
    u32 ink = rgba(SNX_GUI_THEME_YORHA_PAGE, 255);
    ui_fill(list, left, top, (float)width, (float)height, panel);
    char heading[96];
    snprintf(heading, sizeof heading, "Input guide / %s", ykw4_input_family_name(active_family));
    if ((width - 16) / 8 < (int)sizeof heading) heading[(width - 16) / 8] = 0;
    ui_text(list, left + 8, top + 4, 1, heading, ink);
    snprintf(heading, sizeof heading, discord_enabled < 0 ? "F6 hide" : "F6 hide / F7 Discord %s",
             discord_enabled ? "on" : "off");
    if ((width - 16) / 8 < (int)sizeof heading) heading[(width - 16) / 8] = 0;
    ui_text(list, left + 8, top + 22, 1, heading, ink);
    for (int index = 0; index < 10; ++index) {
        float column = left + 8 + index % columns * 96;
        float row = top + 42 + index / columns * 26;
        ui_text(list, column, row + 2, 1, names[index], ink);
        ui_text(list, column + 16, row + 2, 1, ":", ink);
        ui_fill(list, column + 28, row, 56, 21, ink);
        ui_fill(list, column + 29, row + 1, 54, 19, panel);
        int control = control_for(&active_map, buttons[index]);
        if (active_family == YKW4_INPUT_PLAYSTATION && control >= 0 && control < 4)
            face_symbol(list, control, column + 49, row + 3, ink);
        else {
            const char* label = input_prompt_label(active_family, &active_map, buttons[index]);
            ui_text(list, column + 56 - (float)strlen(label) * 4, row + 2, 1, label, ink);
        }
    }
}
