/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "launcher_launch.h"
#include "common/utilities/common.h"
#include <stdlib.h>
char* launcher_game_argument(const char* executable) {
    char *directory, *game, *argument;
    if (!executable || !*executable) return NULL;
    directory = snx_dirname(executable);
    game = directory ? snx_path_join(directory, "YKW4") : NULL;
    argument = game ? snx_format("--game=%s", game) : NULL;
    free(game);
    free(directory);
    return argument;
}
