#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher_binding.h"

static unsigned char identity = 1;
int launcher_binding_identity(unsigned char digest[32]) {
    memset(digest, identity, 32);
    return identity != 0;
}

static unsigned char* bytes(const wchar_t* path, size_t* size) {
    FILE* file = _wfopen(path, L"rb");
    assert(file);
    assert(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 0 && !fseek(file, 0, SEEK_SET));
    unsigned char* data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return data;
}

static void verify_value(HMODULE module) {
    assert(module);
    int (*value)(void) = (int (*)(void))(void*)GetProcAddress(module, "binding_fixture_value");
    assert(value && value() == 427);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    wchar_t fixture[32768], folder[32768], path[32768], scratch[32768], wide_error[512];
    char utf8[131072], error[512];
    assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, fixture, 32768));
    assert(GetTempPathW(32768, folder));
    assert(GetTempFileNameW(folder, L"ykw", 0, path));
    assert(CopyFileW(fixture, path, FALSE));
    assert(launcher_binding_required(path) == 0);
    assert(WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof utf8, NULL, NULL));
    HMODULE legacy = launcher_binding_load(utf8, error, sizeof error);
    verify_value(legacy);
    FreeLibrary(legacy);
    size_t original_size;
    unsigned char* original = bytes(path, &original_size);
    identity = 0;
    assert(!launcher_binding_seal(path, wide_error, 512));
    size_t unchanged_size;
    unsigned char* unchanged = bytes(path, &unchanged_size);
    assert(original_size == unchanged_size && !memcmp(original, unchanged, original_size));
    free(unchanged);
    identity = 1;
    assert(launcher_binding_seal(path, wide_error, 512));
    assert(launcher_binding_kind(path) == 1 && launcher_binding_required(path) == 1);
    size_t sealed_size;
    unsigned char* sealed = bytes(path, &sealed_size);
    assert(sealed_size != original_size || memcmp(sealed, original, original_size));
    assert(launcher_binding_seal(path, wide_error, 512));
    unchanged = bytes(path, &unchanged_size);
    assert(sealed_size == unchanged_size && !memcmp(sealed, unchanged, sealed_size));
    free(unchanged);
    identity = 2;
    assert(!launcher_binding_load(utf8, error, sizeof error));
    assert(!launcher_binding_seal(path, wide_error, 512));
    identity = 1;
    HMODULE bound = launcher_binding_load(utf8, error, sizeof error);
    if (!bound) fprintf(stderr, "%s\n", error);
    verify_value(bound);
    assert(GetModuleFileNameW(bound, scratch, 32768));
    assert(GetFileAttributesW(scratch) != INVALID_FILE_ATTRIBUTES);
    HANDLE writer = CreateFileW(scratch, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    assert(writer == INVALID_HANDLE_VALUE);
    launcher_binding_release();
    assert(GetFileAttributesW(scratch) == INVALID_FILE_ATTRIBUTES);
    FILE* corrupt = _wfopen(path, L"r+b");
    assert(corrupt && !fseek(corrupt, -1, SEEK_END));
    assert(fputc(sealed[sealed_size - 1] ^ 1, corrupt) != EOF);
    fclose(corrupt);
    assert(!launcher_binding_load(utf8, error, sizeof error));
    corrupt = _wfopen(path, L"wb");
    assert(corrupt && fwrite(sealed + 16, 1, sealed_size - 16, corrupt) == sealed_size - 16);
    fclose(corrupt);
    assert(launcher_binding_required(path) == -1);
    assert(!launcher_binding_load(utf8, error, sizeof error));
    assert(DeleteFileW(path));
    FILE* malformed = _wfopen(path, L"wb");
    assert(malformed && fwrite("MZnot-a-PE", 1, 10, malformed) == 10);
    assert(!fclose(malformed));
    assert(launcher_binding_kind(path) == -1 && launcher_binding_required(path) == -1);
    assert(!launcher_binding_seal(path, wide_error, 512));
    assert(!launcher_binding_load(utf8, error, sizeof error));
    assert(DeleteFileW(path));
    IMAGE_DOS_HEADER invalid_dos = {0};
    invalid_dos.e_magic = IMAGE_DOS_SIGNATURE;
    invalid_dos.e_lfanew = 0x7fffffff;
    malformed = _wfopen(path, L"wb");
    assert(malformed && fwrite(&invalid_dos, 1, sizeof invalid_dos, malformed) == sizeof invalid_dos);
    assert(!fclose(malformed));
    assert(launcher_binding_required(path) == -1);
    assert(!launcher_binding_seal(path, wide_error, 512));
    assert(DeleteFileW(path));
    assert(launcher_binding_required(path) == 1);
    assert(!launcher_binding_seal(path, wide_error, 512));
    free(original); free(sealed);
    puts("Legacy unchanged; bound module loads locally; changed identity, tampering and stripped header rejected; temporary plaintext removed");
    return 0;
}
