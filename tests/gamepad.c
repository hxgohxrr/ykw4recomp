/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "nx/runtime/platform/gamepad.h"
#include "nx/runtime/options.h"
#include "nx/runtime/platform/plat.h"
#include "common/utilities/common.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int g_verbose;
char g_confdir[1024], g_exedir[1024];
void logf_(int lvl, const char* fmt, ...) {
    (void)lvl;
    (void)fmt;
}
void pm_lock(PMutex* m) { (void)m; }
void pm_unlock(PMutex* m) { (void)m; }

static char stored[64];
void ini_set(const char* k, const char* v) { snprintf(stored, sizeof stored, "%s=%s", k, v); }

static int checks;
#define CHECK(e)                                                                                                       \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(e)) {                                                                                                    \
            fprintf(stderr, "gamepad.c:%d: check failed: %s\n", __LINE__, #e);                                         \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)


enum { A = 1, B = 2, X = 4, Y = 8, ZL = 1 << 8, ZR = 1 << 9, PLUS = 1 << 10, LEFT = 1 << 12, UP = 1 << 13 };

#define XBOX      0x045e, 0x02ea
#define SWITCH    0x057e, 0x2009
#define SONY      0x054c, 0x09cc
#define GAMECUBE  0x057e, 0x0337
#define JOY_LEFT  0x057e, 0x2006
#define JOY_RIGHT 0x057e, 0x2007
#define PADS_IDS  "0x045e/0x02ea,0x057e/0x2009,0x054c/0x09cc,0x057e/0x0337,0x057e/0x2006,0x057e/0x2007"


static void pump(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_EVENT_GAMEPAD_ADDED)
            gamepad_open(e.gdevice.which);
        else if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
            gamepad_close(e.gdevice.which);
}

static Uint32 motors[16];
static int pads;
static bool SDLCALL rumble(void* pad, Uint16 low, Uint16 high) {
    *(Uint32*)pad = low | (Uint32)high << 16;
    return true;
}
static bool SDLCALL sensors_enabled(void* pad, bool enabled) {
    (void)pad;
    (void)enabled;
    return true;
}

static SDL_Joystick* attach(Uint16 vendor, Uint16 product) {
    SDL_VirtualJoystickDesc d;
    SDL_Joystick* j;
    int a;
    SDL_INIT_INTERFACE(&d);
    d.Rumble = rumble;
    d.userdata = &motors[pads++];
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.vendor_id = vendor;
    d.product_id = product;
    j = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&d));
    CHECK(j != NULL);

    for (a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++)
        SDL_SetJoystickVirtualAxis(j, a, a >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? SDL_JOYSTICK_AXIS_MIN : 0);
    pump();
    return j;
}
static SDL_Joystick* attach_motion(void) {
    static const SDL_VirtualJoystickSensorDesc sensors[6] = {{SDL_SENSOR_ACCEL, 60.0f},   {SDL_SENSOR_GYRO, 60.0f},
                                                             {SDL_SENSOR_ACCEL_L, 60.0f}, {SDL_SENSOR_GYRO_L, 60.0f},
                                                             {SDL_SENSOR_ACCEL_R, 60.0f}, {SDL_SENSOR_GYRO_R, 60.0f}};
    SDL_VirtualJoystickDesc d;
    SDL_Joystick* j;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.nsensors = 6;
    d.sensors = sensors;
    d.SetSensorsEnabled = sensors_enabled;
    d.vendor_id = 0x057e;
    d.product_id = 0x2009;
    j = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&d));
    CHECK(j != NULL);
    pump();
    return j;
}
static u64 buttons(void) {
    u64 b;
    i32 s[4];
    pump();
    gamepad_state(&b, s);
    return b;
}

static u64 with(SDL_Joystick* j, SDL_GamepadButton button) {
    u64 b;
    SDL_SetJoystickVirtualButton(j, button, true);
    b = buttons();
    SDL_SetJoystickVirtualButton(j, button, false);
    CHECK(buttons() == 0);
    return b;
}
static void sticks(SDL_Joystick* j, Sint16 lx, Sint16 ly, i32 s[4]) {
    u64 b;
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, lx);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, ly);
    pump();
    gamepad_state(&b, s);
}

int main(void) {
    SDL_Joystick *xbox, *pro, *sony, *cube, *fifth, *motion;
    SDL_Window* w;
    i32 s[4];
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, PADS_IDS);
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    w = SDL_CreateWindow("gamepad", 64, 64, 0);
    CHECK(w != NULL && SDL_GetKeyboardFocus() == w);


    {
        char line[160];
        char *root = snx_temp_dir(NULL), *copy = snx_path_join(root, ".YKW4Recomp");
        char *config = snx_path_join(copy, "config"), *game = snx_path_join(copy, "games/0100DA801624E000");
        char *db = snx_path_join(config, "gamecontrollerdb.txt"), *mapping;
        snprintf(line, sizeof line, "03000000341200007856000000000000,Odd Stick,a:b0,b:b1,x:b2,y:b3,platform:%s,\n",
                 SDL_GetPlatform());
        CHECK(root && !snx_mkdirs(config) && !snx_mkdirs(game) && !snx_write_file(db, line, strlen(line)));
        snprintf(g_exedir, sizeof g_exedir, "%s/", game);
        gamepad_scan();
        mapping = SDL_GetGamepadMappingForGUID(SDL_StringToGUID("03000000341200007856000000000000"));
        CHECK(mapping && strstr(mapping, "Odd Stick"));
        SDL_free(mapping);
        g_exedir[0] = 0;
        CHECK(!snx_remove_tree(root));
        free(db);
        free(game);
        free(config);
        free(copy);
        free(root);
    }


    xbox = attach(XBOX);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_WEST) == Y);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_NORTH) == X);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_START) == PLUS);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_DPAD_LEFT) == LEFT);

    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0);
    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    CHECK(buttons() == (ZL | ZR));
    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    CHECK(buttons() == 0);

    SDL_HideWindow(w);
    CHECK(SDL_GetKeyboardFocus() == NULL);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    CHECK(gamepad_ini("gamepad_background", "1"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    sticks(xbox, SDL_JOYSTICK_AXIS_MAX, 0, s);
    CHECK(s[0] == 32767);
    sticks(xbox, 0, 0, s);
    CHECK(gamepad_ini("gamepad_background", "0") && !gamepad_ini("gamepad", "0"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_EAST) == 0);
    SDL_ShowWindow(w);


    sticks(xbox, SDL_JOYSTICK_AXIS_MAX, 0, s);
    CHECK(s[0] == 32767 && s[1] == 0 && s[2] == 0 && s[3] == 0);
    sticks(xbox, 0, SDL_JOYSTICK_AXIS_MIN, s);
    CHECK(s[0] == 0 && s[1] == 32767);
    sticks(xbox, SDL_JOYSTICK_AXIS_MAX, SDL_JOYSTICK_AXIS_MIN, s);  
    CHECK(s[0] == s[1] && s[0] > 23160 && s[0] < 23180);
    sticks(xbox, 3000, -3000, s);
    CHECK(s[0] == 0 && s[1] == 0);
    sticks(xbox, 16384, 0, s);
    CHECK(s[0] > 12280 && s[0] < 12295 && s[1] == 0);  
    CHECK(gamepad_ini("gamepad_deadzone", "0"));
    sticks(xbox, 3000, -3000, s);
    CHECK(s[0] == 3000 && s[1] == 3000);
    CHECK(gamepad_ini("gamepad_deadzone", "50"));
    sticks(xbox, 16000, 0, s);
    CHECK(s[0] == 0);
    sticks(xbox, SDL_JOYSTICK_AXIS_MAX, 0, s);
    CHECK(s[0] == 32767);
    CHECK(gamepad_ini("gamepad_deadzone", "20"));
    sticks(xbox, 0, 0, s);


    pro = attach(SWITCH);
    CHECK(with(pro, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(with(pro, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(with(pro, SDL_GAMEPAD_BUTTON_NORTH) == X);
    CHECK(with(pro, SDL_GAMEPAD_BUTTON_WEST) == Y);
    sony = attach(SONY);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_WEST) == Y);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_NORTH) == X);
    cube = attach(GAMECUBE);
    CHECK(with(cube, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(with(cube, SDL_GAMEPAD_BUTTON_WEST) == Y);
    CHECK(with(cube, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(with(cube, SDL_GAMEPAD_BUTTON_NORTH) == X);

    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(gamepad_button(SDL_GetJoystickID(pro), SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_DPAD_UP) == UP);
    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_GUIDE) == 0);


    CHECK(gamepad_ini("gamepad_swap_ab", "1"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == A);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_EAST) == B);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_NORTH) == X);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_WEST) == Y);
    CHECK(with(cube, SDL_GAMEPAD_BUTTON_EAST) == B);
    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_SOUTH) == A);
    CHECK(gamepad_ini("gamepad_swap_ab", "0") && !gamepad_ini("gamepad", "1") && !gamepad_ini("gamepad_buttons", "1"));


    CHECK(gamepad_ini("gamepad_map", "south:A east:B lt:R start:None"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == A);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_EAST) == B);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_WEST) == Y);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_SOUTH) == A);
    CHECK(with(sony, SDL_GAMEPAD_BUTTON_NORTH) == X);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_START) == 0);
    CHECK(gamepad_button(SDL_GetJoystickID(xbox), SDL_GAMEPAD_BUTTON_EAST) == B);
    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    CHECK(buttons() == (1ull << 7) && (gamepad_held() & 1u << SNX_PAD_LT));  
    SDL_SetJoystickVirtualAxis(xbox, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    CHECK(buttons() == 0 && !gamepad_held());
    CHECK(gamepad_ini("gamepad_swap_ab", "1"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == B);
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(gamepad_ini("gamepad_swap_ab", "0"));
    CHECK(gamepad_ini("gamepad_map", "south:Q"));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == A);
    CHECK(gamepad_ini("gamepad_map", ""));
    CHECK(with(xbox, SDL_GAMEPAD_BUTTON_SOUTH) == B);


    gamepad_rumble(0x4000u | 0x1000u << 16, 0x2000u | 0x8000u << 16);
    CHECK(motors[0] == (0x4000u | 0x8000u << 16) && motors[1] == motors[0] && motors[3] == motors[0]);
    CHECK(gamepad_ini("gamepad_rumble", "50"));
    gamepad_rumble(0x4000u | 0x1000u << 16, 0x2000u | 0x8000u << 16);
    CHECK(motors[0] == (0x2000u | 0x4000u << 16));
    CHECK(gamepad_ini("gamepad_rumble", "0"));  
    gamepad_rumble(0xFFFFu | 0xFFFFu << 16, 0);
    CHECK(motors[0] == 0 && motors[2] == 0);
    CHECK(gamepad_ini("gamepad_rumble", "100"));
    gamepad_rumble(0xFFFFu, 0);
    CHECK(motors[0] == 0xFFFFu);
    gamepad_rumble(0, 0);  
    CHECK(motors[0] == 0);


    SDL_SetJoystickVirtualButton(xbox, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SDL_SetJoystickVirtualButton(pro, SDL_GAMEPAD_BUTTON_EAST, true);
    CHECK(buttons() == (A | B));
    SDL_SetJoystickVirtualButton(xbox, SDL_GAMEPAD_BUTTON_SOUTH, false);
    SDL_SetJoystickVirtualButton(pro, SDL_GAMEPAD_BUTTON_EAST, false);
    sticks(pro, SDL_JOYSTICK_AXIS_MIN, 0, s);
    CHECK(s[0] <= -32767 && s[1] == 0);
    sticks(pro, 0, 0, s);


    fifth = attach(SWITCH);
    CHECK(with(fifth, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(gamepad_name(4) && !strcmp(gamepad_name(4), SDL_GetJoystickName(fifth)) && !gamepad_name(5));
    SDL_DetachVirtualJoystick(SDL_GetJoystickID(sony));
    pump();
    CHECK(with(fifth, SDL_GAMEPAD_BUTTON_EAST) == A);
    CHECK(!strcmp(gamepad_name(0), SDL_GetJoystickName(xbox)) && !strcmp(gamepad_name(3), SDL_GetJoystickName(fifth)));
    SDL_DetachVirtualJoystick(SDL_GetJoystickID(pro));
    pump();
    CHECK(!strcmp(gamepad_name(2), SDL_GetJoystickName(fifth)) && !gamepad_name(3));


    motion = attach_motion();
    CHECK(gamepad_slot_connected(1));
    {
        const float accel_left[3] = {9.80665f, 19.6133f, 29.41995f};
        const float gyro_left[3] = {1.0f, 2.0f, 3.0f};
        const float accel_right[3] = {-9.80665f, 0.0f, 9.80665f};
        const float gyro_right[3] = {-1.0f, 0.0f, 2.0f};
        PadMotion sample[2];
        Uint64 now = SDL_GetTicksNS();
        CHECK(SDL_SendJoystickVirtualSensorData(motion, SDL_SENSOR_ACCEL_L, now, accel_left, 3));
        CHECK(SDL_SendJoystickVirtualSensorData(motion, SDL_SENSOR_GYRO_L, now, gyro_left, 3));
        CHECK(SDL_SendJoystickVirtualSensorData(motion, SDL_SENSOR_ACCEL_R, now, accel_right, 3));
        CHECK(SDL_SendJoystickVirtualSensorData(motion, SDL_SENSOR_GYRO_R, now, gyro_right, 3));
        SDL_UpdateGamepads();
        gamepad_motion(1, sample);
        CHECK(sample[0].valid && sample[1].valid);
        CHECK(fabsf(sample[0].acceleration[0] + 1.0f) < 0.001f && fabsf(sample[0].acceleration[1] - 3.0f) < 0.001f &&
              fabsf(sample[0].acceleration[2] + 2.0f) < 0.001f);
        CHECK(fabsf(sample[0].angular_velocity[0] - 1.0f) < 0.001f &&
              fabsf(sample[0].angular_velocity[1] + 3.0f) < 0.001f &&
              fabsf(sample[0].angular_velocity[2] - 2.0f) < 0.001f);
        CHECK(fabsf(sample[1].acceleration[0] - 1.0f) < 0.001f && fabsf(sample[1].acceleration[1] - 1.0f) < 0.001f &&
              sample[1].acceleration[2] == 0.0f);
        gamepad_motion(0, sample);
        CHECK(!sample[0].valid && !sample[1].valid);
    }
    {
        SDL_Joystick* left = attach(JOY_LEFT);
        SDL_Joystick* right = attach(JOY_RIGHT);
        SDL_JoystickID left_id = SDL_GetJoystickID(left), right_id = SDL_GetJoystickID(right);
        CHECK(gamepad_slot_type(2) == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT);
        CHECK(gamepad_slot_type(5) == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT);
        CHECK(gamepad_button(left_id, SDL_GAMEPAD_BUTTON_LEFT_STICK) == (1ull << 4));
        CHECK(gamepad_button(left_id, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) == (1ull << 6));
        CHECK(gamepad_button(left_id, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) == (1ull << 8));
        CHECK(gamepad_button(right_id, SDL_GAMEPAD_BUTTON_LEFT_STICK) == (1ull << 5));
        CHECK(gamepad_button(right_id, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) == (1ull << 7));
        CHECK(gamepad_button(right_id, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) == (1ull << 9));
        SDL_DetachVirtualJoystick(left_id);
        SDL_DetachVirtualJoystick(right_id);
        pump();
    }


    gamepad_config()->deadzone = 35;
    gamepad_store(&gamepad_config()->deadzone);
    CHECK(!strcmp(stored, "gamepad_deadzone=35"));
    gamepad_config()->swap_ab = 1;
    gamepad_store(&gamepad_config()->swap_ab);
    CHECK(!strcmp(stored, "gamepad_swap_ab=1"));
    gamepad_config()->swap_ab = 0;
    gamepad_store(&gamepad_config()->swap_ab);
    gamepad_config()->background = 1;  
    gamepad_store(&gamepad_config()->background);
    CHECK(!strcmp(stored, "gamepad_background=1") &&
          SDL_GetHintBoolean(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, false));


    {
        SDL_Joystick* extra[5];
        SDL_JoystickID old_id = SDL_GetJoystickID(motion);
        int i;
        for (i = 0; i < 5; i++) extra[i] = attach(SWITCH);
        CHECK(gamepad_slot_connected(0) && gamepad_slot_connected(7));
        CHECK(gamepad_slot_id(1) == old_id);
        CHECK(with(extra[4], SDL_GAMEPAD_BUTTON_SOUTH) == 0);
        SDL_DetachVirtualJoystick(SDL_GetJoystickID(motion));
        pump();
        CHECK(gamepad_slot_id(1) == SDL_GetJoystickID(extra[4]) && gamepad_slot_id(1) != old_id);
        CHECK(with(extra[4], SDL_GAMEPAD_BUTTON_SOUTH) == B);
    }


    {
        SDL_Joystick* first = SDL_GetJoystickFromID(gamepad_slot_id(0));
        SDL_Joystick* second = SDL_GetJoystickFromID(gamepad_slot_id(1));
        SDL_Joystick *named, *twin;
        char guid[40];
        CHECK(first && second);
        CHECK(gamepad_ini("gamepad_p1_swap_ab", "1"));
        CHECK(with(first, SDL_GAMEPAD_BUTTON_SOUTH) == A && with(second, SDL_GAMEPAD_BUTTON_SOUTH) == B);
        CHECK(gamepad_ini("gamepad_swap_ab", "1") && with(second, SDL_GAMEPAD_BUTTON_SOUTH) == A);  
        CHECK(gamepad_ini("gamepad_p2_swap_ab", "0") && with(second, SDL_GAMEPAD_BUTTON_SOUTH) == B);  
        CHECK(gamepad_ini("gamepad_swap_ab", "0") && gamepad_ini("gamepad_p1_swap_ab", "-1") &&
              gamepad_ini("gamepad_p2_swap_ab", "-1"));
        CHECK(gamepad_ini("gamepad_p2_map", "south:X"));
        CHECK(with(second, SDL_GAMEPAD_BUTTON_SOUTH) == X && with(first, SDL_GAMEPAD_BUTTON_SOUTH) == B);
        CHECK(gamepad_ini("gamepad_p2_map", "south:Q") && with(second, SDL_GAMEPAD_BUTTON_SOUTH) == X);  
        CHECK(gamepad_ini("gamepad_p2_map", "") && with(second, SDL_GAMEPAD_BUTTON_SOUTH) == B);  
        CHECK(gamepad_ini("gamepad_p1_rumble", "0"));
        gamepad_rumble_slot(0, 0xFFFFu, 0);
        CHECK(motors[0] == 0);
        CHECK(gamepad_ini("gamepad_p1_rumble", "-10"));
        gamepad_rumble_slot(0, 0xFFFFu, 0);
        CHECK(motors[0] == 0xFFFFu);
        CHECK(gamepad_player_seen(1) && gamepad_ini("gamepad_p2_connected", "0") && !gamepad_player_seen(1));
        CHECK(gamepad_ini("gamepad_p1_connected", "0") && gamepad_player_seen(0));  
        CHECK(gamepad_player_kind(1, SNX_PAD_FULL_KEY) == SNX_PAD_FULL_KEY);
        CHECK(gamepad_ini("gamepad_p2_type", "3") && gamepad_player_kind(1, SNX_PAD_FULL_KEY) == SNX_PAD_JOY_LEFT);
        CHECK(gamepad_ini("gamepad_p1_connected", "1") && gamepad_ini("gamepad_p2_connected", "1") &&
              gamepad_ini("gamepad_p2_type", "0"));

        SDL_DetachVirtualJoystick(gamepad_slot_id(0));
        SDL_DetachVirtualJoystick(gamepad_slot_id(2));
        pump();
        CHECK(!gamepad_slot_connected(0) && !gamepad_slot_connected(2));
        named = attach(SONY);
        CHECK(gamepad_slot_id(0) == SDL_GetJoystickID(named));  
        SDL_GUIDToString(SDL_GetJoystickGUID(named), guid, sizeof guid);
        SDL_DetachVirtualJoystick(SDL_GetJoystickID(named));
        pump();
        CHECK(gamepad_ini("gamepad_p3_device", guid));
        named = attach(SONY);
        CHECK(gamepad_slot_id(2) == SDL_GetJoystickID(named) && !gamepad_slot_connected(0));
        twin = attach(SONY);
        CHECK(gamepad_slot_id(0) == SDL_GetJoystickID(twin));
        CHECK(gamepad_ini("gamepad_p3_device", ""));
    }

    SDL_DestroyWindow(w);
    SDL_Quit();
    printf("gamepad: %d checks\n", checks);
    return 0;
}
