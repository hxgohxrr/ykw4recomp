/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_ICON_H
#define YKW4_LAUNCHER_ICON_H
#include <windows.h>
void launcher_icon_apply(HWND window, int has_dlc);
void launcher_branding_init(HWND window);
int launcher_branding_draw(const DRAWITEMSTRUCT* item);
int launcher_icon_has_dlc(const wchar_t* base);
#endif
