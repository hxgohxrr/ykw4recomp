/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GNU_ARM_EXEC_H
#define SNX_GNU_ARM_EXEC_H
#include "gnuabi/process/process.h"
#include "common/cpu/arm/arm.h"
typedef struct GnuArmExec GnuArmExec;

int gnu_arm_run(GnuProcess* process);
#endif
