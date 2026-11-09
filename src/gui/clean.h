/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_CLEAN_H
#define SNX_GUI_CLEAN_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* text;       
    int count, done;  
} GuiCleaned;

size_t gui_clean_deprecated(char* text, size_t size);

int gui_clean_inis(GuiCleaned* done, const char* folder);

void gui_cleaned_add(GuiCleaned* done, bool ok, const char* file, const char* what);

const char* gui_cleaned_entry(const char* at, bool* ok, char* file, char* what, size_t size);
void gui_cleaned_free(GuiCleaned* done);
#endif
