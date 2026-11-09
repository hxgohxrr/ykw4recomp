/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_PAD_SDL_H
#define SNX_PAD_SDL_H
#include <SDL3/SDL.h>
#include "common/utilities/pad_map.h"

#define SNX_PAD_TRIGGER 8000  

int snx_pad_of_button(int button);

int snx_pad_held(SDL_Gamepad* g, int control);

void snx_pad_labels(SDL_Gamepad* g, int labels[4]);
int snx_pad_symmetric(SDL_Gamepad* g);
#endif
