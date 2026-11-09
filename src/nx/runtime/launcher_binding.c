#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher_binding.h"

static const unsigned char binding_magic[16] = {'Y','K','W','4','B','I','N','D',1,0,0,0,0,0,0,0};
static const DWORD binding_limit = 1024u * 1024u * 1024u;
static HANDLE loaded_file = INVALID_HANDLE_VALUE;
static HMODULE loaded_module;
static int cleanup_registered;

static HANDLE read_file(const wchar_t* path, LARGE_INTEGER* size) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return file;
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !GetFileSizeEx(file, size) || size->QuadPart < 2) {
        CloseHandle(file);
        return INVALID_HANDLE_VALUE;
    }
    return file;
}

static int file_kind(HANDLE file) {
    unsigned char header[sizeof binding_magic];
    DWORD count = 0;
    if (!ReadFile(file, header, sizeof header, &count, NULL)) return -1;
    if (count >= 2 && header[0] == 'M' && header[1] == 'Z') {
        IMAGE_DOS_HEADER dos;
        IMAGE_NT_HEADERS64 image;
        LARGE_INTEGER size, offset;
        offset.QuadPart = 0;
        if (!GetFileSizeEx(file, &size) || !SetFilePointerEx(file, offset, NULL, FILE_BEGIN) ||
            !ReadFile(file, &dos, sizeof dos, &count, NULL) || count != sizeof dos ||
            dos.e_lfanew < (LONG)sizeof dos ||
            (LONGLONG)dos.e_lfanew > size.QuadPart - (LONGLONG)sizeof image) return -1;
        offset.QuadPart = dos.e_lfanew;
        if (!SetFilePointerEx(file, offset, NULL, FILE_BEGIN) ||
            !ReadFile(file, &image, sizeof image, &count, NULL) || count != sizeof image ||
            image.Signature != IMAGE_NT_SIGNATURE || image.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            !(image.FileHeader.Characteristics & IMAGE_FILE_DLL) ||
            image.FileHeader.SizeOfOptionalHeader < sizeof image.OptionalHeader ||
            image.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return -1;
        return 0;
    }
    return count == sizeof header && !memcmp(header, binding_magic, sizeof header) ? 1 : -1;
}

int launcher_binding_kind(const wchar_t* path) {
    LARGE_INTEGER size;
    HANDLE file = read_file(path, &size);
    if (file == INVALID_HANDLE_VALUE) return -1;
    int kind = file_kind(file);
    CloseHandle(file);
    return kind;
}

int launcher_binding_required(const wchar_t* installed) {
    DWORD attributes = GetFileAttributesW(installed);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 1 : -1;
    }
    return launcher_binding_kind(installed);
}

static int read_bytes(HANDLE file, DWORD offset, DWORD length, DATA_BLOB* bytes) {
    LARGE_INTEGER start;
    start.QuadPart = offset;
    if (!SetFilePointerEx(file, start, NULL, FILE_BEGIN)) return 0;
    bytes->pbData = malloc(length);
    bytes->cbData = length;
    DWORD count = 0;
    if (!bytes->pbData || !ReadFile(file, bytes->pbData, length, &count, NULL) || count != length) {
        free(bytes->pbData);
        bytes->pbData = NULL;
        return 0;
    }
    return 1;
}

static void wipe_bytes(DATA_BLOB* bytes, int local) {
    if (!bytes->pbData) return;
    SecureZeroMemory(bytes->pbData, bytes->cbData);
    if (local) LocalFree(bytes->pbData); else free(bytes->pbData);
    bytes->pbData = NULL;
}

static int unprotect(HANDLE file, DWORD length, DATA_BLOB* plain) {
    DATA_BLOB sealed = {0}, entropy;
    unsigned char digest[32];
    if (!launcher_binding_identity(digest)) return 0;
    entropy.pbData = digest;
    entropy.cbData = sizeof digest;
    int ok = read_bytes(file, sizeof binding_magic, length - sizeof binding_magic, &sealed) &&
        CryptUnprotectData(&sealed, NULL, &entropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, plain);
    wipe_bytes(&sealed, 0);
    SecureZeroMemory(digest, sizeof digest);
    if (ok && (plain->cbData < 2 || plain->pbData[0] != 'M' || plain->pbData[1] != 'Z')) {
        wipe_bytes(plain, 1);
        ok = 0;
    }
    return ok;
}

static HANDLE private_file(const wchar_t* base, wchar_t* path, size_t capacity, int temporary) {
    unsigned char random[16];
    if (BCryptGenRandom(NULL, random, sizeof random, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return INVALID_HANDLE_VALUE;
    wchar_t suffix[33];
    for (unsigned index = 0; index < sizeof random; index++) swprintf(suffix + index * 2, 3, L"%02x", random[index]);
    if (wcslen(base) + 52 >= capacity) return INVALID_HANDLE_VALUE;
    swprintf(path, capacity, L"%ls.ykw4-%ls.tmp.dll", base, suffix);
    PSECURITY_DESCRIPTOR descriptor = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &descriptor, NULL))
        return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES security = {sizeof security, descriptor, FALSE};
    DWORD flags = FILE_ATTRIBUTE_TEMPORARY | (temporary ? FILE_FLAG_DELETE_ON_CLOSE : 0);
    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              &security, CREATE_NEW, flags, NULL);
    LocalFree(descriptor);
    return file;
}

int launcher_binding_seal(const wchar_t* path, wchar_t* error, size_t capacity) {
    const wchar_t* message = L"No se pudo vincular el modulo. La instalacion anterior no se ha modificado.";
    LARGE_INTEGER size;
    DATA_BLOB plain = {0}, sealed = {0};
    HANDLE source = read_file(path, &size), target = INVALID_HANDLE_VALUE;
    wchar_t* temporary = calloc(32768, sizeof(wchar_t));
    int ok = 0, created = 0, plain_local = 0;
    if (source == INVALID_HANDLE_VALUE || !temporary || size.QuadPart > binding_limit) goto done;
    int kind = file_kind(source);
    if (kind == 1) {
        plain_local = 1;
        ok = size.QuadPart > (LONGLONG)sizeof binding_magic && unprotect(source, (DWORD)size.QuadPart, &plain);
        goto done;
    }
    if (kind != 0) goto done;
    unsigned char digest[32];
    if (!launcher_binding_identity(digest)) {
        message = L"No se pudo leer un UUID SMBIOS valido. No se ha instalado ni vinculado el modulo.";
        goto done;
    }
    DATA_BLOB entropy = {sizeof digest, digest};
    int encrypted = read_bytes(source, 0, (DWORD)size.QuadPart, &plain) &&
        CryptProtectData(&plain, L"YKW4 local module v1", &entropy, NULL, NULL,
                         CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN, &sealed);
    SecureZeroMemory(digest, sizeof digest);
    if (!encrypted || sealed.cbData > binding_limit - sizeof binding_magic) goto done;
    target = private_file(path, temporary, 32768, 0);
    if (target == INVALID_HANDLE_VALUE) goto done;
    created = 1;
    DWORD written = 0;
    int saved = WriteFile(target, binding_magic, sizeof binding_magic, &written, NULL) && written == sizeof binding_magic &&
        WriteFile(target, sealed.pbData, sealed.cbData, &written, NULL) && written == sealed.cbData && FlushFileBuffers(target);
    CloseHandle(target); target = INVALID_HANDLE_VALUE;
    CloseHandle(source); source = INVALID_HANDLE_VALUE;
    if (saved) ok = ReplaceFileW(path, temporary, NULL, 0, NULL, NULL) != 0;
done:
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (target != INVALID_HANDLE_VALUE) CloseHandle(target);
    if (created && !ok) DeleteFileW(temporary);
    free(temporary);
    wipe_bytes(&plain, plain_local);
    wipe_bytes(&sealed, 1);
    if (!ok && capacity) swprintf(error, capacity, L"%ls", message);
    return ok;
}

void launcher_binding_release(void) {
    if (loaded_module) FreeLibrary(loaded_module);
    loaded_module = NULL;
    if (loaded_file != INVALID_HANDLE_VALUE) CloseHandle(loaded_file);
    loaded_file = INVALID_HANDLE_VALUE;
}

void* launcher_binding_load(const char* path, char* error, size_t capacity) {
    const char* message = "Cannot open the prepared module.";
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    wchar_t* wide = count > 0 ? calloc((size_t)count, sizeof(wchar_t)) : NULL;
    wchar_t* temporary = calloc(32768, sizeof(wchar_t));
    DATA_BLOB plain = {0};
    LARGE_INTEGER size;
    HANDLE source = INVALID_HANDLE_VALUE, target = INVALID_HANDLE_VALUE;
    HMODULE module = NULL;
    if (!wide || !temporary || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count)) goto done;
    source = read_file(wide, &size);
    if (source == INVALID_HANDLE_VALUE) goto done;
    int kind = file_kind(source);
    if (!kind) {
        module = LoadLibraryExW(wide, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        message = "Cannot load the legacy module or one of its dependencies.";
        goto done;
    }
    message = "This module is damaged or bound to another PC or Windows installation. Prepare it again from your own game files in a new folder. Saves have not been changed.";
    if (kind != 1 || size.QuadPart <= (LONGLONG)sizeof binding_magic || size.QuadPart > binding_limit ||
        !unprotect(source, (DWORD)size.QuadPart, &plain)) goto done;
    message = "Cannot create the private temporary module. Check disk space and folder permissions.";
    wchar_t* folder = calloc(32768, sizeof(wchar_t));
    DWORD length = folder ? GetTempPathW(32768, folder) : 0;
    if (length && length < 32768 && !loaded_module) target = private_file(folder, temporary, 32768, 0);
    free(folder);
    if (target == INVALID_HANDLE_VALUE) goto done;
    DWORD written = 0;
    if (!WriteFile(target, plain.pbData, plain.cbData, &written, NULL) || written != plain.cbData || !FlushFileBuffers(target)) {
        CloseHandle(target); target = INVALID_HANDLE_VALUE;
        DeleteFileW(temporary);
        goto done;
    }
    CloseHandle(target);
    target = CreateFileW(temporary, GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (target == INVALID_HANDLE_VALUE) { DeleteFileW(temporary); goto done; }
    message = "Cannot load the decrypted module or one of its dependencies.";
    module = LoadLibraryExW(temporary, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module) {
        if (!cleanup_registered && atexit(launcher_binding_release)) { FreeLibrary(module); module = NULL; goto done; }
        cleanup_registered = 1;
        loaded_module = module;
        loaded_file = target;
        target = INVALID_HANDLE_VALUE;
    }
done:
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (target != INVALID_HANDLE_VALUE) CloseHandle(target);
    wipe_bytes(&plain, 1);
    free(wide); free(temporary);
    if (!module && capacity) snprintf(error, capacity, "%s (Windows error %lu)", message, (unsigned long)GetLastError());
    return module;
}
