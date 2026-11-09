#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "launcher_binding.h"

int main(void) {
    unsigned char firmware[40] = {1, 25, 0, 0};
    unsigned char first[32], second[32];
    for (unsigned index = 0; index < 16; index++) firmware[8 + index] = (unsigned char)(index + 1);
    assert(launcher_binding_firmware_identity(firmware, sizeof firmware, first));
    assert(launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    assert(!memcmp(first, second, 32));
    firmware[8] ^= 1;
    assert(launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    assert(memcmp(first, second, 32));
    memset(firmware + 8, 0, 16);
    assert(!launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    memset(firmware + 8, 255, 16);
    assert(!launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    assert(!launcher_binding_firmware_identity(firmware, 24, second));
    firmware[1] = 3;
    assert(!launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    firmware[1] = 25;
    firmware[0] = 0;
    memset(firmware + 25, 1, sizeof firmware - 25);
    assert(!launcher_binding_firmware_identity(firmware, sizeof firmware, second));
    assert(!launcher_binding_firmware_identity(firmware, 0, second));
    int available = launcher_binding_identity(first);
    if (available) {
        assert(launcher_binding_identity(second));
        assert(!memcmp(first, second, 32));
    }
    puts(available ? "SMBIOS parser and local identity OK; no identifiers printed" : "SMBIOS parser OK; local firmware identity unavailable");
    return 0;
}
