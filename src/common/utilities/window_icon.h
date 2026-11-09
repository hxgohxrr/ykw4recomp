/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_WINDOW_ICON_H
#define SNX_WINDOW_ICON_H
#include <SDL3/SDL.h>


SDL_Surface* snx_window_icon_surface(void);

void snx_window_icon(SDL_Window* window);
#endif
