/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_SETTINGS_H
#define SNX_GUI_SETTINGS_H
#include <stddef.h>
#include "gui/theme.h"

#ifndef SNX_GUI_HAS_GCCJIT
#define SNX_GUI_HAS_GCCJIT 0
#endif
#define GUI_SETTINGS_OWN 2  

enum { GUI_SETTING_PAGE = 1, GUI_SETTING_ALL };
#define GUI_SETTING_TEXT 192  
typedef struct {
    int word;                            
    char text[GUI_SETTING_TEXT];         
    char start[GUI_SETTING_TEXT];        
    char option[GUI_SETTING_TEXT + 72];  
    const char* key;                     
    const char** values;                 
    const char* fallback;  
    int kind;              
    int first, count;      
} GuiSetting;
typedef struct {
    GuiThemeKey* keys;   
    GuiSetting* values;  
    int count;           
} GuiSettings;

int gui_settings_read(GuiSettings* settings, const char* list);
void gui_settings_free(GuiSettings* settings);

int gui_settings_set(GuiSettings* settings, const char* key, const char* value);

void gui_settings_mark(GuiSettings* settings);

void gui_settings_load(GuiSettings* settings, const char* path);

int gui_settings_save(const GuiSettings* settings, const char* path);

int gui_settings_unread(GuiSettings* settings, const char* path, char* out, size_t size);

void gui_settings_reset(GuiSettings* settings, int row);

int gui_settings_step_all(GuiSettings* settings, int row, int step);

void gui_settings_sync(GuiSettings* settings);

void gui_settings_shown(const GuiSettings* settings, int row, char* out, size_t size);

const char* gui_settings_value(const GuiSettings* settings, const char* key);

size_t gui_settings_options(GuiSettings* settings, int launch, int prepared, const char** argv);

const char* gui_settings_backend(const GuiSettings* settings);
#endif
