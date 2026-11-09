/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_PLACE_H
#define SNX_GUI_PLACE_H

typedef struct {
    int x, y, w, h;
} GuiRect;
typedef struct {
    int width, height;   
    int cw, ch;          
    int tabs, tabs_w;    
    int title, title_w;  
    int title_beside;    
    int tips;            
    int foot;            
    int keys_w;          
    int list_cols;       
    int fixed;           
    int status;          
} GuiPlaceIn;
typedef struct {
    GuiRect head;  
    GuiRect tabs, title, tips, list, side, status, keys;
    int stacked;  
} GuiPlace;
#define GUI_PLACE_SIDE_MIN 24  
#define GUI_PLACE_LIST_MIN 20  
void gui_place(const GuiPlaceIn* in, GuiPlace* out);

GuiRect gui_rect(int x, int y, int w, int h);

int gui_place_list(int height, int item, int pitch, int count, int focus, int* first);

int gui_place_pitch(int height, int count, int least, int most);
#endif
