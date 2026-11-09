/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_EDITOR_H
#define SNX_GUI_EDITOR_H
#include <stdbool.h>
#include <stddef.h>
#include "gui/model.h"
#include "gui/theme.h"

typedef struct {
    const GuiThemeKey* keys;  
    int count;
    char* object;        
    const char* action;  

    GuiTheme theme;
    GuiThemeFile file;
    char id[64];
    int row, top;  
    bool typing;   
    char text[80];
    bool dirty, leaving;  
    int preview;          
    char note[320];       
    bool flat;            
    int step;             
    bool picking;         
    int channel;          
    GuiRgb was;           
    bool was_dirty;       
} GuiEditor;

enum { GUI_PICK_RED, GUI_PICK_GREEN, GUI_PICK_BLUE, GUI_PICK_HEX, GUI_PICK_ROWS };
#define GUI_EDITOR_SAVE_ROW 0  
#define GUI_EDITOR_FOLDER   1
int gui_editor_rows(const GuiEditor* editor);

const GuiThemeKey* gui_editor_key_of(const GuiEditor* editor, int row);

void gui_editor_start(GuiEditor* editor, const GuiTheme* theme, GuiThemeFile* file, const char* id);

void gui_editor_form(GuiEditor* editor, const GuiThemeKey* keys, int count, void* object, const char* action);
void gui_editor_stop(GuiEditor* editor);

typedef enum { GUI_EDITOR_STAY, GUI_EDITOR_LEAVE, GUI_EDITOR_SAVE, GUI_EDITOR_ACTION } GuiEditorDo;

GuiEditorDo gui_editor_key(GuiEditor* editor, GuiKey key);

GuiRgb* gui_editor_colour(GuiEditor* editor);

void gui_editor_pick(GuiEditor* editor, int channel, int value);

void gui_editor_pick_end(GuiEditor* editor, bool keep);

void gui_editor_type(GuiEditor* editor, const char* text);
void gui_editor_erase(GuiEditor* editor);

void gui_editor_label(const GuiEditor* editor, int row, char* out, size_t size);
void gui_editor_value(const GuiEditor* editor, int row, char* out, size_t size);

int gui_editor_save(GuiEditor* editor, const char* themes_dir);

void gui_editor_scroll(GuiEditor* editor, int rows);
#endif
