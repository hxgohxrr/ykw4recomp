/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/utilities/ini_lines.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


static void read_all(const char* text, size_t size, char* out, size_t room) {
    static char copy[512];
    SnxIni ini;
    char *key, *value;
    size_t used = 0;
    assert(size < sizeof copy);
    memcpy(copy, text, size);
    copy[size] = 0;
    out[0] = 0;
    snx_ini_start(&ini, copy);
    while (snx_ini_next(&ini, &key, &value)) {
        int n = sprintf(out + used, "%s/%s=%s\n", ini.section, key, value);
        assert(n > 0 && used + (size_t)n < room);
        used += (size_t)n;
    }
}
#define CHECK(text, expect)                                                                                            \
    do {                                                                                                               \
        char got[512];                                                                                                 \
        read_all(text, sizeof text - 1, got, sizeof got);                                                              \
        if (strcmp(got, expect)) {                                                                                     \
            printf("%s\n--- read as ---\n%s--- not ---\n%s", text, got, expect);                                       \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

int main(void) {

    CHECK("a = 1\n[one]\n; note\n# note\n\nnothing here\nb=2\n[two]\nc= 3 ", "/a=1\none/b=2\ntwo/c=3\n");

    CHECK("\t key \t=\t \"x = y\" \r\n", "/key=\"x = y\"\n");
    CHECK("k=\r\n=v\n", "/k=\n/=v\n");

    CHECK("k=a\rb\r\n", "/k=a\rb\n");

    CHECK("\xEF\xBB\xBF[s]\nk=v\n\xEF\xBB\xBFx=y\n", "s/k=v\ns/\xEF\xBB\xBFx=y\n");

    CHECK("[s]\n[open\nk=1\n[s]\n[0123456789abcdef0123456789abcdef]\nk=2\n[0123456789abcdef0123456789abcde]\nk=3\n",
          "/k=1\n/k=2\n0123456789abcdef0123456789abcde/k=3\n");

    CHECK("a=1\nb=2\0c=3\n", "/a=1\n/b=2\n");
    CHECK("", "");
    puts("ini_lines passed");
    return 0;
}
