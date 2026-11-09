/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nx/runtime/launcher_launch.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char* argument = launcher_game_argument("A:/Games/My game/YKW4Recomp.exe");
    assert(argument && !strcmp(argument, "--game=A:/Games/My game/YKW4"));
    free(argument);
    argument = launcher_game_argument("C:/Juegos/Espa\xc3\xb1ol/YKW4Recomp.exe");
    assert(argument && !strcmp(argument, "--game=C:/Juegos/Espa\xc3\xb1ol/YKW4"));
    free(argument);
    assert(!launcher_game_argument(NULL));
    assert(!launcher_game_argument(""));
    puts("Native launcher selects YKW4 independently of its executable name.");
    return 0;
}
