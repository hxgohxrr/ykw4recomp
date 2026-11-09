/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_VPAD_H
#define SNX_VPAD_H
#include "nx/runtime/platform/ui.h"
#include "nx/runtime/ini.h"


enum {
    SNX_VPAD_L,
    SNX_VPAD_R,
    SNX_VPAD_ZL,
    SNX_VPAD_ZR,
    SNX_VPAD_MINUS,
    SNX_VPAD_PLUS,
    SNX_VPAD_CAPTURE,
    SNX_VPAD_HOME,
    SNX_VPAD_L3,
    SNX_VPAD_R3,
    SNX_VPAD_DPAD,
    SNX_VPAD_STICK_L,
    SNX_VPAD_STICK_R,
    SNX_VPAD_A,
    SNX_VPAD_B,
    SNX_VPAD_X,
    SNX_VPAD_Y,
    SNX_VPAD_COUNT
};

#define SNX_VPAD_DO_MENU 1u  
#define SNX_VPAD_DO_SHOT 2u  
#define SNX_VPAD_DO_BUZZ 4u  

typedef struct {
    float x, y;  
    int size;    
    int shown;
} SnxVpadPlace;
typedef struct {
    int on;          
    int scale;       
    int opacity;     
    int snap;        
    int rel_center;  
    int dpad_slide;  
    int haptics;     
    SnxVpadPlace at[SNX_VPAD_COUNT];
} SnxVpadConfig;

SnxVpadConfig* vpad_config(void);
#define SNX_VPAD_SIZE_MIN 50
#define SNX_VPAD_SIZE_MAX 200

void vpad_defaults(SnxVpadConfig* c);

void vpad_reset_layout(void);
extern const IniOpt g_vpad_ini[];        
extern const IniOpt g_vpad_shown_ini[];  

#define SNX_VPAD_PAGE    "On-screen controls"
#define SNX_VPAD_BUTTONS "Buttons"

int vpad_ini(const char* key, const char* value);

void vpad_set_store(void (*store)(const char* key, const char* value));

void vpad_store(const int* setting);
void vpad_store_control(int control);
void vpad_store_shown(int control);


int vpad_down(u64 id, float x, float y, const int safe[4]);
void vpad_move(u64 id, float x, float y, const int safe[4]);
int vpad_up(u64 id);
void vpad_release(void);  

void vpad_state(u64* buttons, i32 sticks[4]);
unsigned vpad_actions(void);  


void vpad_edit(int on);
int vpad_editing(void);
int vpad_shown(void);  


void vpad_draw(SnxUiList* l, const int safe[4]);
#endif
