#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>
#include "launcher_binding.h"

int launcher_binding_firmware_identity(const unsigned char* firmware, size_t size, unsigned char digest[32]) {
    size_t offset = 0;
    while (offset + 4 <= size) {
        unsigned type = firmware[offset], length = firmware[offset + 1];
        if (length < 4 || length > size - offset) return 0;
        if (type == 1) {
            if (length < 25) return 0;
            const unsigned char* uuid = firmware + offset + 8;
            unsigned zero = 0, ones = 0;
            for (unsigned index = 0; index < 16; index++) {
                zero |= uuid[index];
                ones |= uuid[index] ^ 255u;
            }
            if (!zero || !ones) return 0;
            unsigned char input[48] = "YKW4 local module binding v1";
            memcpy(input + 32, uuid, 16);
            return BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0, input, sizeof input, digest, 32) >= 0;
        }
        if (type == 127) return 0;
        offset += length;
        while (offset + 1 < size && (firmware[offset] || firmware[offset + 1])) offset++;
        if (offset + 1 >= size) return 0;
        offset += 2;
    }
    return 0;
}

int launcher_binding_identity(unsigned char digest[32]) {
    const DWORD provider = 0x52534d42;
    UINT size = GetSystemFirmwareTable(provider, 0, NULL, 0);
    if (size < 8 || size > 16 * 1024 * 1024) return 0;
    unsigned char* raw = malloc(size);
    int ok = 0;
    if (raw && GetSystemFirmwareTable(provider, 0, raw, size) == size) {
        DWORD length;
        memcpy(&length, raw + 4, sizeof length);
        if (length <= size - 8) ok = launcher_binding_firmware_identity(raw + 8, length, digest);
    }
    if (raw) { SecureZeroMemory(raw, size); free(raw); }
    return ok;
}
