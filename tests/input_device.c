/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nx/runtime/platform/input_device.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Ykw4InputDevice state = {0};
    assert(state.family == YKW4_INPUT_KEYBOARD && state.controller == 0);
    assert(ykw4_input_select(&state, YKW4_INPUT_XBOX, 41));
    assert(!ykw4_input_select(&state, YKW4_INPUT_XBOX, 41));
    assert(!ykw4_input_remove(&state, 42));
    assert(state.family == YKW4_INPUT_XBOX);
    assert(ykw4_input_select(&state, YKW4_INPUT_PLAYSTATION, 42));
    assert(!ykw4_input_remove(&state, 41));
    assert(ykw4_input_remove(&state, 42));
    assert(state.family == YKW4_INPUT_KEYBOARD && !state.controller);
    assert(!ykw4_input_remove(&state, 0));
    assert(!ykw4_input_axis_active(6000, 0));
    assert(!ykw4_input_axis_active(-6000, 0));
    assert(ykw4_input_axis_active(-32768, 0));
    assert(ykw4_input_axis_active(32767, 0));
    assert(!ykw4_input_axis_active(0, 1));
    assert(!ykw4_input_axis_active(-32768, 1));
    assert(ykw4_input_axis_active(32767, 1));
    assert(ykw4_input_select(&state, YKW4_INPUT_SWITCH, 43));
    assert(ykw4_input_select(&state, YKW4_INPUT_GENERIC, 44));
    assert(ykw4_input_select(&state, YKW4_INPUT_KEYBOARD, 0));
    puts("input device selection passed");
    return 0;
}
