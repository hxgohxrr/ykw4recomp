/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include "launcher_setup.h"
typedef struct {
    wchar_t source[32768], target[32768], previous[32768];
    int saved, published;
} UpdateFile;
int launcher_publish_update(const wchar_t* base, const wchar_t* data, const wchar_t* code, const wchar_t* backup) {
    UpdateFile* files = calloc(4, sizeof *files);
    if (!files) return 0;
    swprintf(files[0].source, 32768, L"%ls\\exefs", data);
    swprintf(files[0].target, 32768, L"%lsYKW4_data\\exefs", base);
    swprintf(files[1].source, 32768, L"%ls\\romfs.bin", data);
    swprintf(files[1].target, 32768, L"%lsYKW4_data\\romfs.bin", base);
    wcsncpy(files[2].source, code, 32767);
    swprintf(files[2].target, 32768, L"%lsYKW4_code.dll", base);
    int count = 3;
    swprintf(files[3].source, 32768, L"%ls\\system\\archives", data);
    if (GetFileAttributesW(files[3].source) != INVALID_FILE_ATTRIBUTES) {
        swprintf(files[3].target, 32768, L"%lsYKW4_data\\system", base);
        if (!CreateDirectoryW(files[3].target, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) { free(files); return 0; }
        swprintf(files[3].target, 32768, L"%lsYKW4_data\\system\\archives", base);
        count = 4;
    }
    int ok = 0;
    for (int i = 0; i < count; i++) {
        swprintf(files[i].previous, 32768, L"%ls\\%d", backup, i);
        if (GetFileAttributesW(files[i].source) == INVALID_FILE_ATTRIBUTES ||
            (i < 3 && GetFileAttributesW(files[i].target) == INVALID_FILE_ATTRIBUTES)) goto done;
    }
    if (!CreateDirectoryW(backup, NULL)) goto done;
    for (int i = 0; i < count; i++) {
        if (GetFileAttributesW(files[i].target) != INVALID_FILE_ATTRIBUTES) {
            if (!MoveFileW(files[i].target, files[i].previous)) goto rollback;
            files[i].saved = 1;
        }
        if (!MoveFileW(files[i].source, files[i].target)) goto rollback;
        files[i].published = 1;
    }
    ok = 1; goto done;
rollback:
    for (int i = count - 1; i >= 0; i--) {
        if (files[i].published && !MoveFileW(files[i].target, files[i].source)) continue;
        if (files[i].saved) MoveFileW(files[i].previous, files[i].target);
    }
done:
    free(files); return ok;
}
