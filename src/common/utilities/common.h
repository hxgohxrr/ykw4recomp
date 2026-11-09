/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_TOOL_COMMON_H
#define SNX_TOOL_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


typedef uint32_t SnxU32;
typedef uint64_t SnxU64;
typedef struct {
    const char* repo;  
    const char* build;
    const char* executable;
    const char* source;  
} SnxPaths;

void* snx_alloc(size_t size);
void* snx_realloc(void* ptr, size_t size);
char* snx_strdup(const char* s);
char* snx_format(const char* fmt, ...);
int snx_error(const char* fmt, ...);
char* snx_path_join(const char* a, const char* b);
char* snx_dirname(const char* path);
char* snx_absolute(const char* path);
char* snx_executable(const char* argv0);
int snx_is_file(const char* path);
int snx_is_dir(const char* path);
int snx_is_link(const char* path);

int snx_path_exists(const char* path);
int snx_mkdirs(const char* path);
int snx_remove_tree(const char* path);
int snx_copy_file(const char* src, const char* dst);
int snx_copy_tree(const char* src, const char* dst);

int snx_link_file(const char* src, const char* dst);
int snx_path_within(const char* path, const char* parent);
int snx_replace(const char* src, const char* dst);
int snx_list_dir(const char* path, char*** names, size_t* count);
void snx_free_strings(char** strings, size_t count);
unsigned char* snx_read_file(const char* path, size_t* size);
int snx_write_file(const char* path, const void* data, size_t size);
int snx_write_changed(const char* path, const void* data, size_t size);
char* snx_temp_dir(const char* parent);
char* snx_user_data_dir(void);

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
#define SNX_XDG 1
#endif
enum { SNX_USER_DATA, SNX_USER_CONFIG, SNX_USER_STATE, SNX_USER_CACHE };

#define SNX_PORTABLE_DIR ".YKW4Recomp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define SNX_IOS 1
#endif
#endif

#define SNX_ANDROID_DIR "/sdcard/YKW4Recomp"

char* snx_user_dir(int kind);

int snx_app_dir(const char* folder);

char* snx_app_kind_dir(const char* app, int kind);

char* snx_user_path(const char* path, int kind);

char* snx_menu_dir(const char* folder, int kind);

int snx_left_behind(const char* old, const char* now);

int snx_launcher_name(const char* name);

#define SNX_GAME_FILES 4
typedef struct {
    const char* name;
    int kind;
} SnxGameFile;
extern const SnxGameFile snx_game_files[SNX_GAME_FILES];

char* snx_work_dir(const char* data_dir);

char* snx_data_dir(const char* program_dir);

char* snx_checkout(const char* checkout, const char* build, const char* program_dir);

#define SNX_CHECKOUT_FILES 5
extern const char* const snx_checkout_files[SNX_CHECKOUT_FILES];
char* snx_checkout_now(const char* data, int file);
void snx_checkout_left(const char* checkout, const char* data);

char* snx_dump_name(const char* input);

const char* snx_folder_unfit(const char* folder, int run);
SnxU64 snx_free_space(const char* path);

int snx_same_disk(const char* a, const char* b);
SnxU64 snx_file_size(const char* path);
int snx_make_executable(const char* path);
unsigned snx_cpu_count(void);

double snx_seconds(void);
void snx_tool_console(void);
char* snx_find_tool(const char* name);

int snx_run(const char* const* argv, const char* cwd, const char* home, char** output);

int snx_run_ticking(const char* const* argv, const char* cwd, const char* home, char** output, void (*tick)(void*),
                    void* data);

int snx_run_stage(const char* const* argv, const char* cwd, const char* stage);

int snx_run_debug_stage(const char* const* argv, const char* cwd, const char* stage);
FILE* snx_fopen(const char* path, const char* mode);


int snx_nacp_name(const unsigned char* nacp, size_t size, char* name, size_t name_size);

#define SNX_NPDM_DEFAULT_TITLE (((SnxU64)0x01000000UL << 32) | 0x1000)

SnxU64 snx_npdm_title(const unsigned char* npdm, size_t size);

int snx_title_text(SnxU64 id, char* out);
#endif
