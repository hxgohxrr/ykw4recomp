/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GUI_RECENT_H
#define SNX_GUI_RECENT_H
#include <stddef.h>


#define GUI_RECENT_MAX 5
typedef struct {
    char path[1024];  
    char name[128];   
    int driver;       
    int prepared;     
} GuiRecentGame;
typedef struct {
    GuiRecentGame game[GUI_RECENT_MAX];
    int count;
} GuiRecent;


int gui_recent_add(GuiRecent* recent, const char* path, int driver, int prepared);
void gui_recent_remove(GuiRecent* recent, int index);

int gui_recent_rename(GuiRecent* recent, const char* path, const char* name);

void gui_recent_load(GuiRecent* recent, const char* file);
int gui_recent_save(const GuiRecent* recent, const char* file);
#endif
