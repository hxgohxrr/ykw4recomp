/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GAMEPAD_H
#define SNX_GAMEPAD_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/runtime/ini.h"
#include "common/utilities/pad_map.h"
#include "nx/hle/services/hid.h"

#define SNX_GAMEPADS 8  

void gamepad_open(u32 id);
void gamepad_close(u32 id);

void gamepad_scan(void);
void gamepad_detect(u32 id);

void gamepad_state(u64* buttons, i32 sticks[4]);
void gamepad_state_slot(int slot, u64* buttons, i32 sticks[4]);
int gamepad_slot_type(int slot);  
int gamepad_slot_connected(int slot);
u32 gamepad_slot_id(int slot);  
void gamepad_motion(int slot, PadMotion out[2]);

u64 gamepad_button(u32 id, int button);

void gamepad_rumble(u32 left, u32 right);

void gamepad_rumble_slot(int slot, u32 left, u32 right);

typedef struct {
    int swap_ab;      
    int deadzone;     
    int rumble;       
    int background;   
    int joycon_pair;  
    SnxPadMap own;    
} SnxGamepadConfig;
SnxGamepadConfig* gamepad_config(void);
void gamepad_store(const int* setting);     
void gamepad_store_map(void);               
void gamepad_store_player_map(int player);  

int gamepad_layout(int i, int labels[4], int* symmetric);
unsigned gamepad_held(void);  

const char* gamepad_name(int i);
extern const IniOpt g_gamepad_ini[];      
#define SNX_GAMEPAD_PAGE "Controllers"    
#define SNX_GAMEPAD_MAP  "Button map"     
extern const IniOpt g_gamepad_map_ini[];  

typedef struct {
    char device[48];  
    int connected;    
    int type;         
    int swap_ab;      
    int rumble;       
    int has_map;      
    SnxPadMap own;    
} SnxPlayerConfig;
SnxPlayerConfig* gamepad_player(int player);  
int gamepad_player_seen(int slot);            
int gamepad_player_kind(int slot, int kind);  
extern const IniOpt g_gamepad_players_ini[];  
#define SNX_GAMEPAD_PLAYERS "Player"
int gamepad_ini(const char* key, const char* value);  

#endif
