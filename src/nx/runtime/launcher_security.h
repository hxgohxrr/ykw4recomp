/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_SECURITY_H
#define YKW4_LAUNCHER_SECURITY_H
#include <windows.h>
void launcher_security_status(HWND window, int control);
void launcher_security_help(HWND owner, const wchar_t* folder);
#endif
