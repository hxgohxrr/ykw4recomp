#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher_binding.h"
#include "launcher_desktop.h"
#include "nx/hle/filesystem/fs.h"
#include "nx/loader/loader.h"
#ifdef _WIN32
#include <windows.h>
#endif

int launcher_local_command(int argc, char** argv) {
    if (argc == 3 && !strcmp(argv[1], "--launcher-validate-exefs")) {
        SnxNpdm metadata;
        char error[512];
        if (!snx_load_npdm(argv[2], &metadata, error, sizeof error) && metadata.title_id == 0x010086c00af7c000ull) return 0;
        fputs("ExeFS inválido o no compatible con YKW4++.\n", stderr);
        return 1;
    }
    if (argc == 2 && !strcmp(argv[1], "--machine-status")) {
        unsigned char digest[32];
        int available = launcher_binding_identity(digest);
        puts(available ? "available" : "unavailable");
        memset(digest, 0, sizeof digest);
        return available ? 0 : 1;
    }
    if (argc == 4 && !strcmp(argv[1], "--launcher-pack-romfs")) {
        volatile int cancelled = 0;
        return fs_pack_romfs_directory(argv[2], argv[3], &cancelled, NULL, NULL) ? 1 : 0;
    }
    if (argc < 2 || (strcmp(argv[1], "--binding-status") && strcmp(argv[1], "--binding-check") && strcmp(argv[1], "--binding-seal"))) return -1;
    int seal = !strcmp(argv[1], "--binding-seal");
    if (argc != (seal ? 4 : 3) || (seal && strcmp(argv[3], "--accept-local-binding"))) {
        fputs("La vinculación requiere una ruta y --accept-local-binding.\n", stderr);
        return 1;
    }
#ifdef _WIN32
    wchar_t path[32768], error[1024] = {0};
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[2], -1, path, 32768)) return 1;
#else
    const char* path = argv[2];
    char error[1024] = {0};
#endif
    if (!strcmp(argv[1], "--binding-status")) {
        int kind = launcher_binding_required(path);
        puts(kind == 0 ? "legacy" : kind == 1 ? "bound-or-new" : "invalid");
        return kind < 0;
    }
    if (seal) {
        if (launcher_binding_seal(path, error, sizeof error / sizeof *error)) return 0;
#ifdef _WIN32
        char message[4096];
        if (WideCharToMultiByte(CP_UTF8, 0, error, -1, message, sizeof message, NULL, NULL)) fprintf(stderr, "%s\n", message);
#else
        fprintf(stderr, "%s\n", error);
#endif
        return 1;
    }
    char message[1024] = {0};
    void* module = launcher_binding_load(argv[2], message, sizeof message);
    if (!module) fprintf(stderr, "%s\n", message);
    launcher_binding_release();
    return module ? 0 : 1;
}
