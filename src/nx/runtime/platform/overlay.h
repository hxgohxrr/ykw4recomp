/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_OVERLAY_H
#define SNX_OVERLAY_H
#include "nx/runtime/platform/ui.h"

#define OVERLAY_COLS     40  
#define OVERLAY_COLS_MAX 72  
#define OVERLAY_ROWS     16  
enum { OVERLAY_NONE, OVERLAY_CLOSE, OVERLAY_EXIT, OVERLAY_EDIT };


int overlay_shown(void);
void overlay_toggle(void);

void overlay_set_display_scale(float scale);

void overlay_place(int sw, int sh, int* x, int* y, int* scale);
int overlay_select(void);       
void overlay_move(int rows);    
void overlay_scroll(int rows);  
void overlay_side(int step);    
int overlay_back(void);         

int overlay_pad(u64 button);

#define SNX_OVERLAY_REPEAT_DELAY 400
#define SNX_OVERLAY_REPEAT_MS    120
int overlay_controls(u64 buttons, const i32 sticks[4], u64 ms);

int overlay_listening(void);
void overlay_press(int control);

int overlay_tap(float x, float y, int sw, int sh);

void overlay_pointer_down(u64 id, float x, float y, const int safe[4]);
void overlay_pointer_move(u64 id, float x, float y, const int safe[4]);
int overlay_pointer_up(u64 id, float x, float y, int sw, int sh);
void overlay_pointer_cancel(u64 id);
void overlay_pointers_reset(void);
void overlay_draw(SnxUiList* l, int sw, int sh);

int overlay_cols(int sw, int scale);

typedef struct {
    float x, y, w, h;
} SnxOverlayRect;
enum { SNX_OVERLAY_ERROR_SHARE, SNX_OVERLAY_ERROR_COPY, SNX_OVERLAY_ERROR_CLOSE, SNX_OVERLAY_ERROR_ITEMS };
typedef struct {
    const char *name, *msg, *how;
    const char* log;   
    const char* said;  
    int focus;         

    SnxOverlayRect link, items[SNX_OVERLAY_ERROR_ITEMS];
} SnxOverlayError;
void overlay_error_draw(SnxUiList* l, int sw, int sh, SnxOverlayError* e);

int overlay_error_hit(const SnxOverlayRect* r, float x, float y);

#endif
