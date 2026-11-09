/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GUI_BROWSER_H
#define SNX_GUI_BROWSER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char** names;  
    char** shown;  
    char** paths;  
    size_t count;
} GuiGames;

enum { GUI_GAMES_RUN = 1, GUI_GAMES_CIA = 2 };

bool gui_games_add(GuiGames* games, const char* directory, int kinds, char* error, size_t size);

bool gui_games_add_deep(GuiGames* games, const char* directory, int kinds, int depth, char* error, size_t size);

bool gui_has_folders(const char* directory, int kinds);

bool gui_games_scan(GuiGames* games, const char* directory, bool cia, char* error, size_t size);
void gui_games_free(GuiGames* games);

uint64_t gui_tree_size(const char* path, const int* stop);

bool gui_is_dump(const char* name);

bool gui_game_folder(const char* path);

bool gui_is_cia(const char* name);
#endif
