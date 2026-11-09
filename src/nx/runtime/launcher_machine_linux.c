#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include "launcher_binding.h"

static int digest_bytes(const void* bytes, size_t size, unsigned char digest[32]) {
    unsigned count = 0;
    return EVP_Digest(bytes, size, digest, &count, EVP_sha256(), NULL) && count == 32;
}

int launcher_binding_firmware_identity(const unsigned char* firmware, size_t size, unsigned char digest[32]) {
    size_t offset = 0;
    while (offset + 4 <= size) {
        unsigned type = firmware[offset], length = firmware[offset + 1];
        if (length < 4 || length > size - offset) return 0;
        if (type == 1) {
            if (length < 25) return 0;
            unsigned zero = 0, ones = 0;
            for (unsigned index = 0; index < 16; index++) {
                zero |= firmware[offset + 8 + index];
                ones |= firmware[offset + 8 + index] ^ 255u;
            }
            if (!zero || !ones) return 0;
            unsigned char input[48] = "YKW4 local module binding v1";
            memcpy(input + 32, firmware + offset + 8, 16);
            return digest_bytes(input, sizeof input, digest);
        }
        if (type == 127) return 0;
        offset += length;
        while (offset + 1 < size && (firmware[offset] || firmware[offset + 1])) offset++;
        if (offset + 1 >= size) return 0;
        offset += 2;
    }
    return 0;
}

int launcher_binding_identity_mode(unsigned mode, unsigned char digest[32]) {
    if (mode > 2) return 0;
    if (mode != 2) {
        FILE* firmware = fopen("/sys/class/dmi/id/product_uuid", "rb");
        char uuid[38] = {0};
        size_t length = firmware ? fread(uuid, 1, sizeof uuid - 1, firmware) : 0;
        if (firmware) fclose(firmware);
        unsigned char record[27] = {1, 25, 0, 0};
        int valid = length == 36 || (length == 37 && uuid[36] == '\n');
        unsigned index = 0;
        for (unsigned offset = 0; valid && offset < 36;) {
            if (offset == 8 || offset == 13 || offset == 18 || offset == 23) {
                if (uuid[offset++] != '-') valid = 0;
                continue;
            }
            unsigned value = 0;
            for (unsigned digit = 0; digit < 2; digit++) {
                unsigned character = (unsigned char)uuid[offset++];
                if (character >= 'A' && character <= 'F') character += 'a' - 'A';
                if (character >= '0' && character <= '9') value = value * 16 + character - '0';
                else if (character >= 'a' && character <= 'f') value = value * 16 + character - 'a' + 10;
                else valid = 0;
            }
            record[8 + index++] = (unsigned char)value;
        }
        if (valid && launcher_binding_firmware_identity(record, sizeof record, digest)) return 1;
        if (mode == 1) return 0;
    }
    unsigned char input[64] = "YKW4 Linux local binding v2";
    FILE* file = fopen("/etc/machine-id", "rb");
    char text[34] = {0};
    size_t count = file ? fread(text, 1, sizeof text - 1, file) : 0;
    if (file) fclose(file);
    if (count != 32 && !(count == 33 && text[32] == '\n')) return 0;
    unsigned nonzero = 0;
    for (unsigned index = 0; index < 32; index++) {
        unsigned value = (unsigned char)text[index];
        if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) return 0;
        nonzero |= value != '0';
    }
    if (!nonzero) return 0;
    memcpy(input + 32, text, 32);
    return digest_bytes(input, sizeof input, digest) ? 2 : 0;
}

int launcher_binding_identity(unsigned char digest[32]) {
    return launcher_binding_identity_mode(0, digest) != 0;
}
