/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/utilities/common.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(snx_launcher_name("YKW4Recomp") && snx_launcher_name("YKW4Recomp-mariko"));
    assert(snx_launcher_name("YKW4Recomp-x86_64") && snx_launcher_name("ykw4recomp"));
    assert(snx_launcher_name("YKW4Recomp.real"));
    assert(!snx_launcher_name("Catherine") && !snx_launcher_name("scarlet") && !snx_launcher_name(""));
    assert(!snx_launcher_name("YKW4Recompgame") && !snx_launcher_name("my YKW4Recomp"));
    puts("launcher names passed");
    return 0;
}
