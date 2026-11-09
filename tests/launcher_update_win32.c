/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/nx/runtime/launcher_update.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void write_file(const wchar_t* path, const char* text) {
    FILE* f = _wfopen(path, L"wb"); assert(f); assert(fputs(text, f) >= 0); assert(!fclose(f));
}
static void check_file(const wchar_t* path, const char* text) {
    char buffer[64] = {0}; FILE* f = _wfopen(path, L"rb"); assert(f);
    assert(fread(buffer, 1, sizeof buffer - 1, f) == strlen(text)); fclose(f); assert(!strcmp(buffer, text));
}
int main(int argc, char** argv) {
    assert(argc == 2);
    wchar_t root[2048], base[2048], data[2048], backup[2048], code[2048], path[2048];
    assert(MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, root, 2048));
    for (int scenario = 0; scenario < 6; scenario++) {
        swprintf(base, 2048, L"%ls\\%lu-%llu-%d\\", root, GetCurrentProcessId(), GetTickCount64(), scenario);
        assert(CreateDirectoryW(base, NULL));
        swprintf(path, 2048, L"%lsYKW4_data", base); assert(CreateDirectoryW(path, NULL));
        swprintf(path, 2048, L"%lsYKW4_data\\exefs", base); assert(CreateDirectoryW(path, NULL));
        swprintf(path, 2048, L"%lsYKW4_data\\exefs\\main", base); write_file(path, "old-code");
        swprintf(path, 2048, L"%lsYKW4_data\\save", base); write_file(path, "save-untouched");
        swprintf(path, 2048, L"%lsYKW4_data\\romfs.bin", base); write_file(path, "old-romfs");
        swprintf(path, 2048, L"%lsYKW4_code.dll", base); write_file(path, "old-module");
        swprintf(data, 2048, L"%lsstaged", base); assert(CreateDirectoryW(data, NULL));
        swprintf(path, 2048, L"%ls\\exefs", data); assert(CreateDirectoryW(path, NULL));
        swprintf(path, 2048, L"%ls\\exefs\\main", data); write_file(path, "new-code");
        swprintf(path, 2048, L"%ls\\romfs.bin", data); write_file(path, "new-romfs");
        swprintf(code, 2048, L"%lsnew.dll", base); if (scenario != 1) write_file(code, "new-module");
        if (scenario >= 3) {
            swprintf(path, 2048, L"%ls\\system", data); assert(CreateDirectoryW(path, NULL));
            swprintf(path, 2048, L"%ls\\system\\archives", data); assert(CreateDirectoryW(path, NULL));
            swprintf(path, 2048, L"%ls\\system\\archives\\new.romfs", data); write_file(path, "new-dlc");
            if (scenario >= 4) {
                swprintf(path, 2048, L"%lsYKW4_data\\system", base); assert(CreateDirectoryW(path, NULL));
                swprintf(path, 2048, L"%lsYKW4_data\\system\\archives", base); assert(CreateDirectoryW(path, NULL));
                swprintf(path, 2048, L"%lsYKW4_data\\system\\archives\\old.romfs", base); write_file(path, "old-dlc");
                swprintf(path, 2048, L"%ls\\system\\archives\\old.romfs", data); write_file(path, "old-dlc");
            }
        }
        swprintf(backup, 2048, L"%lsbackup", base);
        HANDLE locked = INVALID_HANDLE_VALUE;
        if (scenario == 2) {
            swprintf(path, 2048, L"%lsYKW4_data\\romfs.bin", base);
            locked = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); assert(locked != INVALID_HANDLE_VALUE);
        }
        if (scenario == 5) {
            swprintf(path, 2048, L"%lsYKW4_data\\system\\archives\\old.romfs", base);
            locked = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); assert(locked != INVALID_HANDLE_VALUE);
        }
        int success = scenario == 0 || scenario == 3 || scenario == 4;
        assert(launcher_publish_update(base, data, code, backup) == success);
        if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
        swprintf(path, 2048, L"%lsYKW4_data\\save", base); check_file(path, "save-untouched");
        swprintf(path, 2048, L"%lsYKW4_data\\exefs\\main", base); check_file(path, !success ? "old-code" : "new-code");
        swprintf(path, 2048, L"%lsYKW4_data\\romfs.bin", base); check_file(path, !success ? "old-romfs" : "new-romfs");
        swprintf(path, 2048, L"%lsYKW4_code.dll", base); check_file(path, !success ? "old-module" : "new-module");
        if (scenario >= 3) {
            swprintf(path, 2048, L"%lsYKW4_data\\system\\archives\\new.romfs", base);
            if (success) check_file(path, "new-dlc");
            else assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
            if (scenario >= 4) {
                swprintf(path, 2048, L"%lsYKW4_data\\system\\archives\\old.romfs", base); check_file(path, "old-dlc");
            }
        }
    }
    puts("launcher_update: publication, personal data preservation, missing input and rollback passed");
    return 0;
}
