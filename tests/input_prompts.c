#include "nx/runtime/platform/input_prompts.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int bounds[4], fills;

void ui_fill(SnxUiList* list, float left, float top, float width, float height, u32 color) {
    (void)color;
    assert(isfinite(left) && isfinite(top) && isfinite(width) && isfinite(height));
    assert(left >= bounds[0] && top >= bounds[1] && width > 0 && height > 0);
    assert(left + width <= bounds[0] + bounds[2] && top + height <= bounds[1] + bounds[3]);
    ++fills;
    ++list->n;
}

float ui_text(SnxUiList* list, float left, float top, int scale, const char* text, u32 color) {
    float width = (float)strlen(text) * 8 * scale;
    if (width) ui_fill(list, left, top, width, 16.0f * scale, color);
    return width;
}

int main(void) {
    SnxPadMap map;
    snx_pad_map_position(&map);
    assert(!strcmp(input_prompt_label(YKW4_INPUT_KEYBOARD, &map, 0), "X/Spc"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_KEYBOARD, &map, 1), "Z"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_XBOX, &map, 0), "B"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_XBOX, &map, 1), "A"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_PLAYSTATION, &map, 0), "Circle"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_PLAYSTATION, &map, 1), "Cross"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_SWITCH, &map, 0), "A"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_GENERIC, &map, 0), "East"));
    snx_pad_map_swap_ab(&map);
    assert(!strcmp(input_prompt_label(YKW4_INPUT_XBOX, &map, 0), "A"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_PLAYSTATION, &map, 0), "Cross"));
    memset(&map, SNX_PAD_NONE, sizeof map);
    map.bit[SNX_PAD_LT] = 0;
    assert(!strcmp(input_prompt_label(YKW4_INPUT_PLAYSTATION, &map, 0), "L2"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_SWITCH, &map, 0), "ZL"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_XBOX, &map, 1), "--"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_XBOX, NULL, 0), "--"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_KEYBOARD, &map, -1), "--"));
    assert(!strcmp(input_prompt_label(YKW4_INPUT_KEYBOARD, &map, 16), "--"));
    snx_pad_map_position(&map);
    static const int screens[][4] = {{0, 0, 1280, 720}, {8, 12, 320, 180}, {0, 0, 208, 200},
                                     {0, 0, 1920, 1080}, {0, 0, 3840, 2160}};
    for (unsigned screen = 0; screen < sizeof screens / sizeof *screens; ++screen) {
        memcpy(bounds, screens[screen], sizeof bounds);
        for (int family = YKW4_INPUT_KEYBOARD; family <= YKW4_INPUT_GENERIC; ++family) {
            SnxUiList list = {0};
            input_prompts_update((Ykw4InputFamily)family, &map, family % 2);
            fills = 0;
            input_prompts_draw(&list, bounds);
            assert(fills > 0 && list.n < 512);
        }
    }
    SnxUiList list = {0};
    g_input_prompts_mode = 0;
    input_prompts_draw(&list, bounds);
    assert(!list.n);
    g_input_prompts_mode = 1;
    bounds[2] = 200;
    input_prompts_draw(&list, bounds);
    assert(!list.n);
    bounds[2] = 320;
    bounds[3] = 80;
    input_prompts_draw(&list, bounds);
    assert(!list.n);
    puts("input_prompts: families, binding inversion, swaps, unmapped controls, keycaps and safe-area bounds passed");
    return 0;
}
