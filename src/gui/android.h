/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_ANDROID_H
#define SNX_GUI_ANDROID_H
#include <stddef.h>
#include <SDL3/SDL_dialog.h>


void gui_android_start(void);

const char* gui_android_own_folder(void);

char* gui_android_document_file(const char* uri);

int gui_android_pick_folder(SDL_DialogFileCallback done, void* data, char* message, size_t size);

const char* gui_android_libraries(void);

int gui_android_play(const char* const* argv, char* message, size_t size);

int gui_android_share(const char* path, char* message, size_t size);

int gui_android_background(char* message, size_t size);

int gui_android_open_data(char* message, size_t size);

void gui_android_running(const char* game, const char* step, int percent);

void gui_android_ran(const char* result);
#endif
