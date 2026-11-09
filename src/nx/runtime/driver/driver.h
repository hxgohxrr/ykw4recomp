/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_DRIVER_H
#define SNX_DRIVER_H

int snx_driver(int argc, char** argv);

int snx_setting_known(const char* key) __attribute__((weak));
#endif
