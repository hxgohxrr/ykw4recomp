/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700  
#include "common/utilities/common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

static char* join(const char* a, const char* b) {
    char* p = snx_path_join(a, b);
    assert(p);
    return p;
}


static char* left_said(const char* checkout, const char* data, const char* file) {
    char* text;
    size_t size;
    fflush(stderr);
    assert(freopen(file, "w", stderr));
    snx_checkout_left(checkout, data);
    fflush(stderr);
    text = (char*)snx_read_file(file, &size);
    assert(text);
    return text;
}

int main(void) {
    char *root = snx_temp_dir(NULL), *checkout, *build, *built, *program, *mount, *copy, *data, *xdg, *expected, *file;
    char *portable, *beside, *said, *line, *cache, *old, *now;
    int i;
    assert(root);

    {
        char* real = realpath(root, NULL);
        assert(real);
        free(root);
        root = real;
    }
    checkout = join(root, "checkout");
    build = join(root, "build");
    built = join(build, "src/gui");
    program = join(root, "program");
    mount = join(root, "mount");
    copy = join(root, "copy");
    xdg = join(root, "xdg");
    cache = join(root, "xdg-cache");
    assert(!snx_mkdirs(checkout) && !snx_mkdirs(built) && !snx_mkdirs(program) && !snx_mkdirs(mount) &&
           !snx_mkdirs(copy));
    file = join(checkout, "CMakeLists.txt");
    assert(!snx_write_file(file, "", 0));
    free(file);
    unsetenv("APPIMAGE");
    unsetenv("APPDIR");
    setenv("XDG_DATA_HOME", xdg, 1);
    setenv("XDG_CACHE_HOME", cache, 1);
#ifdef SNX_XDG
    expected = join(xdg, "YKW4Recomp");
#else

    file = join(root, "home");
    setenv("HOME", file, 1);
    free(file);
    expected = snx_user_data_dir();
    assert(expected);
#endif


    data = snx_data_dir(built);
    assert(data && !strcmp(data, expected));
    free(data);
    data = snx_data_dir(program);
    assert(data && !strcmp(data, expected));
    free(data);
    data = snx_data_dir(NULL);
    assert(data && !strcmp(data, expected));
    free(data);

    file = join(program, "games");
    assert(!snx_mkdirs(file));
    free(file);
    data = snx_data_dir(program);
    assert(data && !strcmp(data, expected));
    free(data);

    beside = join(program, ".YKW4Recomp");
    assert(!snx_mkdirs(beside));
    data = snx_data_dir(program);
    assert(data && !strcmp(data, beside));
    free(data);


    file = join(copy, "YKW4Recomp.AppImage");
    assert(!snx_write_file(file, "", 0));
    setenv("APPIMAGE", file, 1);
    setenv("APPDIR", mount, 1);
    free(file);
    file = join(mount, "bin");
    data = snx_data_dir(file);
    assert(data && !strcmp(data, expected));  
    free(data);
    portable = join(copy, ".YKW4Recomp");
    assert(!snx_mkdirs(portable));
    data = snx_data_dir(file);
    assert(data && !strcmp(data, portable));
    free(data);
    free(file);

    data = snx_data_dir(program);
    assert(data && !strcmp(data, beside));
    free(data);
    free(portable);
    free(beside);
    unsetenv("APPIMAGE");
    unsetenv("APPDIR");


    data = snx_checkout(checkout, build, built);
    assert(data && !strcmp(data, checkout));
    free(data);
    data = snx_checkout(checkout, NULL, program);
    assert(data && !strcmp(data, checkout));
    free(data);

    assert(!snx_checkout(checkout, build, program));

    assert(!snx_checkout(program, NULL, program));
    assert(!snx_checkout(NULL, NULL, program));


    for (i = 0; i < SNX_CHECKOUT_FILES; ++i) {
        now = snx_checkout_now(expected, i);
#ifdef SNX_XDG
        line = !strcmp(snx_checkout_files[i], "work") ? snx_format("%s/YKW4Recomp/work", cache)
                                                      : join(expected, snx_checkout_files[i]);
#else
        line = !strcmp(snx_checkout_files[i], "work") ? join(expected, "cache/work")
                                                      : join(expected, snx_checkout_files[i]);
#endif
        assert(now && !strcmp(now, line));
        free(line);
        free(now);
    }

    file = join(root, "stderr.txt");
    said = left_said(checkout, expected, file);
    assert(!said[0]);
    free(said);
    old = join(checkout, "games");
    assert(!snx_mkdirs(old));
    free(old);
    old = join(checkout, "prod.keys");
    assert(!snx_write_file(old, "", 0));
    free(old);
    said = left_said(checkout, expected, file);
    line = snx_format("YKW4Recomp: %s/games was left by an older version: this one keeps it in %s/games "
                      "(docs/DEPRECATIONS.md)\n"
                      "YKW4Recomp: %s/prod.keys was left by an older version: this one keeps it in %s/prod.keys "
                      "(docs/DEPRECATIONS.md)\n",
                      checkout, expected, checkout, expected);
    assert(!strcmp(said, line));
    free(line);
    free(said);
    old = join(checkout, "games");
    assert(snx_is_dir(old));
    free(old);

    said = left_said(checkout, checkout, file);
    assert(!said[0]);
    free(said);
    free(file);


    {
        char *dir = snx_path_join(root, "fit"), *file = snx_path_join(root, "fit.txt"), **left = NULL;
        size_t n = 0;
        assert(!strcmp(snx_folder_unfit(dir, 0), "is not there"));
        assert(!snx_mkdirs(dir) && !snx_folder_unfit(dir, 1) && !snx_write_file(file, "", 0));
        assert(!strcmp(snx_folder_unfit(file, 0), "is not a folder"));
#ifndef _WIN32
        if (geteuid()) {  
            assert(!chmod(dir, 0555) && !strcmp(snx_folder_unfit(dir, 0), "cannot be written"));
            assert(!chmod(dir, 0755));
        }
#endif
        assert(!snx_list_dir(dir, &left, &n) && !n);  
        snx_free_strings(left, n);
        free(file);
        free(dir);
    }

    {
        static const char* const names[][2] = {{"/dumps/My Game [v1.0].nsp", "My Game v10"},
                                               {"C:\\dumps\\two.dots.xci", "twodots"},
                                               {"/dumps/.nsp", "nsp"},
                                               {"/dumps/[] .xci", "Game"},
                                               {"caf\xc3\xa9 -_1.nsp", "caf\xc3\xa9 -_1"}};
        size_t i;
        for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
            char* name = snx_dump_name(names[i][0]);
            assert(!strcmp(name, names[i][1]));
            free(name);
        }
    }

    assert(snx_same_disk(checkout, program));
    file = join(root, "not/made/yet");
    assert(snx_same_disk(file, checkout));
    free(file);
#ifdef __linux__
    assert(!snx_same_disk(root, "/proc"));
#endif

    puts("snx_data_dir: .YKW4Recomp beside the program or the AppImage, else the user's data folder, for a build "
         "tree's programs too; snx_checkout: a build tree's program's checkout, what an older version kept in it said");
    free(expected);
    free(cache);
    free(xdg);
    free(copy);
    free(mount);
    free(built);
    free(build);
    free(program);
    free(checkout);
    free(root);
    return 0;
}
