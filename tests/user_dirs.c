/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700  
#include "common/utilities/common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char* join(const char* a, const char* b) {
    char* p = snx_path_join(a, b);
    assert(p);
    return p;
}
static void expect_dir(int kind, const char* expected) {
    char* dir = snx_user_dir(kind);
    assert(expected ? dir && !strcmp(dir, expected) : !dir);
    free(dir);
}
static void expect_path(const char* path, int kind, const char* expected) {
    char* moved = snx_user_path(path, kind);
    assert(moved && !strcmp(moved, expected));
    free(moved);
}
static void expect_menu(const char* folder, int kind, const char* expected) {
    char* dir = snx_menu_dir(folder, kind);
    assert(dir && !strcmp(dir, expected));
    free(dir);
}

int main(void) {
    static const char* const vars[] = {"XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME"};
    char *root = snx_temp_dir(NULL), *home, *dir[4], *base, *games, *path;
    int kind;
    assert(root);

    {
        char* real = realpath(root, NULL);
        assert(real);
        free(root);
        root = real;
    }
    home = join(root, "home");
    setenv("HOME", home, 1);
    for (kind = SNX_USER_DATA; kind <= SNX_USER_CACHE; ++kind) unsetenv(vars[kind]);
#ifdef SNX_XDG
    {
        static const char* const defaults[] = {".local/share", ".config", ".local/state", ".cache"};
        char* expected;
        for (kind = SNX_USER_DATA; kind <= SNX_USER_CACHE; ++kind) {

            base = join(home, defaults[kind]);
            expected = join(base, "YKW4Recomp");
            expect_dir(kind, expected);
            setenv(vars[kind], "", 1);
            expect_dir(kind, expected);
            setenv(vars[kind], "relative/folder", 1);
            expect_dir(kind, expected);
            free(expected);
            free(base);

            unsetenv("HOME");
            expect_dir(kind, NULL);
            base = join(root, vars[kind]);
            expected = join(base, "YKW4Recomp");
            setenv(vars[kind], base, 1);
            expect_dir(kind, expected);
            setenv("HOME", home, 1);
            expect_dir(kind, expected);
            free(expected);
            free(base);
        }
    }
#else

    {
        static const char* const names[] = {NULL, "config", "state", "cache"};
        base = snx_user_data_dir();
        assert(base);
        expect_dir(SNX_USER_DATA, base);
        for (kind = SNX_USER_CONFIG; kind <= SNX_USER_CACHE; ++kind) {
            path = join(base, names[kind]);
            expect_dir(kind, path);
            free(path);
        }
        free(base);
    }
#endif

    for (kind = SNX_USER_DATA; kind <= SNX_USER_CACHE; ++kind) {
        base = join(root, vars[kind]);
        setenv(vars[kind], base, 1);
        dir[kind] = snx_user_dir(kind);
        assert(dir[kind]);
        free(base);
    }
    games = join(dir[SNX_USER_DATA], "games");
    assert(!snx_mkdirs(games));
#ifdef SNX_XDG
    {
        char *expected, *link, *cwd;

        path = join(dir[SNX_USER_CONFIG], "games");
        expect_path(games, SNX_USER_CONFIG, path);
        free(path);
        path = join(games, "Game/Game.ini");  
        expected = join(dir[SNX_USER_CONFIG], "games/Game/Game.ini");
        expect_path(path, SNX_USER_CONFIG, expected);
        free(expected);
        free(path);
        path = join(dir[SNX_USER_STATE], "games");
        expect_path(games, SNX_USER_STATE, path);
        free(path);
        path = join(dir[SNX_USER_DATA], "work");
        expected = join(dir[SNX_USER_CACHE], "work");
        expect_path(path, SNX_USER_CACHE, expected);
        free(expected);
        free(path);
        expect_path(dir[SNX_USER_DATA], SNX_USER_CACHE, dir[SNX_USER_CACHE]);

        path = snx_format("%s/", games);
        expected = join(dir[SNX_USER_STATE], "games");
        expect_path(path, SNX_USER_STATE, expected);
        free(path);
        link = join(root, "link");
        assert(!symlink(dir[SNX_USER_DATA], link));
        path = join(link, "games");
        expect_path(path, SNX_USER_STATE, expected);
        free(path);
        free(link);

        cwd = getcwd(NULL, 0);
        assert(cwd && !chdir(games));
        expect_path("./", SNX_USER_STATE, expected);
        expect_path(".", SNX_USER_STATE, expected);
        assert(!chdir(cwd));
        free(cwd);
        free(expected);
    }
#endif

    expect_path(games, SNX_USER_DATA, games);
    path = join(root, "portable/games");
    expect_path(path, SNX_USER_CONFIG, path);
    expect_path(path, SNX_USER_CACHE, path);
    free(path);

    {
        char *expected, *copy = join(root, "portable/.YKW4Recomp");
        path = join(copy, "games/Game");
        expected = join(copy, "config/games/Game");
        expect_path(path, SNX_USER_CONFIG, expected);
        free(expected);
        expected = join(copy, "cache/work");
        free(path);
        path = join(copy, "work");
        expect_path(path, SNX_USER_CACHE, expected);
        expect_path(path, SNX_USER_DATA, path);
        free(expected);
        free(path);
        expected = join(copy, "state");
        expect_path(copy, SNX_USER_STATE, expected);
        assert(snx_app_dir(copy) && !snx_app_dir(root));
        free(expected);
        path = join(root, "portable/.YKW4Recomp-other/games");  
        expect_path(path, SNX_USER_STATE, path);
        free(path);

        expected = join(copy, "config");
        expect_menu(copy, SNX_USER_CONFIG, expected);
        path = join(copy, "games/0100DA801624E000/");
        expect_menu(path, SNX_USER_CONFIG, expected);
        free(path);
        free(expected);
        free(copy);
    }

    {
        char* user = snx_user_dir(SNX_USER_CONFIG);
        assert(user);
        expect_menu(games, SNX_USER_CONFIG, user);
        path = join(root, "portable/games/0100DA801624E000");
        expect_menu(path, SNX_USER_CONFIG, user);
        free(path);
        expect_menu(NULL, SNX_USER_CONFIG, user);
        free(user);
    }
    path = snx_format("%s-other/games", dir[SNX_USER_DATA]);
    expect_path(path, SNX_USER_STATE, path);
    free(path);
    expect_path("games", SNX_USER_CONFIG, "games");
#ifdef SNX_XDG  

    setenv("XDG_CONFIG_HOME", getenv("XDG_DATA_HOME"), 1);
    expect_path(games, SNX_USER_CONFIG, games);
#endif

    puts("snx_user_dir: the XDG variables, their defaults, a relative path ignored; snx_user_path: the installed "
         "and a portable copy's games/ divided, any other folder kept; snx_menu_dir: the launcher's for its games");
    for (kind = SNX_USER_DATA; kind <= SNX_USER_CACHE; ++kind) free(dir[kind]);
    free(games);
    free(home);
    assert(!snx_remove_tree(root));
    free(root);
    return 0;
}
