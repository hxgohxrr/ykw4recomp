/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GNU_EXECUTION_H
#define SNX_GNU_EXECUTION_H
#include "gnuabi/process/process.h"
#include "common/cpu/execution/execution.h"

int gnu_runtime_run(GnuProcess* process);
#endif
