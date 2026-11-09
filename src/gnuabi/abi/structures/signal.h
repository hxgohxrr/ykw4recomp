/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GNU_SIGNAL_LAYOUT_H
#define SNX_GNU_SIGNAL_LAYOUT_H
#include <stdint.h>

typedef struct GnuSignalAction {
    uint64_t handler, flags, restorer, mask;
} GnuSignalAction;
#endif
