/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_APPLE_H
#define SNX_GUI_APPLE_H
#include <stddef.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>


void gui_apple_start(void);

int gui_apple_pick(SDL_Window* window, int folder, SDL_DialogFileCallback done, void* data, char* message,
                   size_t size);

int gui_apple_share(SDL_Window* window, const char* path, char* message, size_t size);

void gui_apple_play(const char* const* argv);

char** gui_apple_played(int* argc);
#endif
