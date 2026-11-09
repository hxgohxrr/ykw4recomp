/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_INPUT_DEVICE_H
#define YKW4_INPUT_DEVICE_H
#include <stdint.h>

typedef enum {
    YKW4_INPUT_KEYBOARD,
    YKW4_INPUT_XBOX,
    YKW4_INPUT_PLAYSTATION,
    YKW4_INPUT_SWITCH,
    YKW4_INPUT_GENERIC
} Ykw4InputFamily;

typedef struct {
    Ykw4InputFamily family;
    uint32_t controller;
} Ykw4InputDevice;

static inline int ykw4_input_select(Ykw4InputDevice* state, Ykw4InputFamily family, uint32_t controller) {
    if (family == YKW4_INPUT_KEYBOARD) controller = 0;
    int changed = state->family != family || state->controller != controller;
    state->family = family;
    state->controller = family == YKW4_INPUT_KEYBOARD ? 0 : controller;
    return changed;
}

static inline int ykw4_input_remove(Ykw4InputDevice* state, uint32_t controller) {
    if (!controller || state->controller != controller) return 0;
    return ykw4_input_select(state, YKW4_INPUT_KEYBOARD, 0);
}

static inline int ykw4_input_axis_active(int value, int trigger) {
    return trigger ? value > 8192 : value > 16000 || value < -16000;
}

static inline const char* ykw4_input_family_name(Ykw4InputFamily family) {
    switch (family) {
        case YKW4_INPUT_KEYBOARD: return "keyboard/mouse";
        case YKW4_INPUT_XBOX: return "Xbox";
        case YKW4_INPUT_PLAYSTATION: return "PlayStation";
        case YKW4_INPUT_SWITCH: return "Switch";
        default: return "generic controller";
    }
}
#endif
