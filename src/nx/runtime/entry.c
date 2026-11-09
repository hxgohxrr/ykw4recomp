/* Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24). */
/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/runtime/main.h"
#include "nx/runtime/driver/driver.h"
#ifdef SNX_WITH_GUI
#include "gui/gui.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include "common/utilities/common.h"
#include "nx/runtime/launcher_launch.h"
#include "nx/runtime/launcher_desktop.h"
#ifdef _WIN32
int ykw4_select_language(int argc);
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(SNX_WITH_GUI)

#include "gui/apple.h"
#include <SDL3/SDL_main.h>
#define SNX_PLAY_AFTER_MENU 1
#endif
int main(int argc, char** argv) {
#if defined(YKW4_DESKTOP_LAUNCHER)
    int local = launcher_local_command(argc, argv);
    if (local >= 0) return local;
#if defined(__linux__)
    int desktop = launcher_linux(argc, argv);
    if (desktop >= 0) return desktop;
#endif
#endif
    char* game_argument = NULL;
    char* game_argv[3];
#ifdef _WIN32
    int selection = ykw4_select_language(argc);
    if (!selection) return 0;
    if (selection == 2) {
        char* executable = snx_executable(argv[0]);
        game_argument = launcher_game_argument(executable);
        free(executable);
        if (!game_argument) return 1;
        game_argv[0] = argv[0];
        game_argv[1] = game_argument;
        game_argv[2] = NULL;
        argv = game_argv;
        argc = 2;
    }
#else
    (void)game_argv;
#endif
#ifdef SNX_WITH_GUI
    snx_menu = snx_gui_main;
#endif

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    int result = snx_driver(argc, argv);
    result = result >= 0 ? result : snx_main(argc, argv);
#ifdef SNX_PLAY_AFTER_MENU
    {
        int played_argc;
        char** played = gui_apple_played(&played_argc);

        if (played) result = snx_main(played_argc, played);
    }
#endif
    free(game_argument);
    return result;
}
