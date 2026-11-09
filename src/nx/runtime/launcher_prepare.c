/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lz4.h"
#include "launcher_setup.h"

static uint32_t word(const unsigned char* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int sha256(const void* bytes, size_t size, unsigned char* digest) {
    return size <= UINT32_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0, (PUCHAR)bytes, (ULONG)size, digest, 32) >= 0;
}
int launcher_prepare_main(const wchar_t* input, const wchar_t* output) {
    unsigned char *raw = NULL, *segments[3] = {0}, hash[32];
    uint32_t sizes[3], flags;
    int ok = 0;
    FILE* file = _wfopen(input, L"rb");
    if (!file) return 0;
    if (_fseeki64(file, 0, SEEK_END)) goto done;
    long long length = _ftelli64(file);
    if (length < 256 || length > 64 * 1024 * 1024 || _fseeki64(file, 0, SEEK_SET)) goto done;
    raw = malloc((size_t)length);
    if (!raw || fread(raw, 1, (size_t)length, file) != (size_t)length) goto done;
    fclose(file); file = NULL;
    if (memcmp(raw, "NSO0", 4)) goto done;
    flags = word(raw + 12);
    for (int i = 0; i < 3; i++) {
        uint32_t start = word(raw + 0x10 + i * 16);
        sizes[i] = word(raw + 0x18 + i * 16);
        uint32_t stored = flags & (1u << i) ? word(raw + 0x60 + i * 4) : sizes[i];
        if (!sizes[i] || sizes[i] > 64 * 1024 * 1024 || start > (uint64_t)length || stored > (uint64_t)length - start) goto done;
        segments[i] = malloc(sizes[i]); if (!segments[i]) goto done;
        if (flags & (1u << i)) {
            if (LZ4_decompress_safe((const char*)raw + start, (char*)segments[i], (int)stored, (int)sizes[i]) != (int)sizes[i]) goto done;
        } else memcpy(segments[i], raw + start, sizes[i]);
        if (!sha256(segments[i], sizes[i], hash) ||
            ((flags & (1u << (i + 3))) && memcmp(hash, raw + 0xa0 + i * 32, 32))) goto done;
    }

    file = _wfopen(output, L"wbx");
    if (!file || fwrite(raw, 1, (size_t)length, file) != (size_t)length) goto done;
    ok = fclose(file) == 0 ? 2 : 0; file = NULL;
done:
    if (file) fclose(file);
    free(raw); for (int i = 0; i < 3; i++) free(segments[i]);
    return ok;
}
