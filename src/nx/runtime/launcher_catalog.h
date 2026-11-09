/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_CATALOG_H
#define YKW4_LAUNCHER_CATALOG_H
#include <windows.h>
typedef struct LauncherMods LauncherMods;
void launcher_catalog(HWND parent, LauncherMods* mods, const wchar_t* root);
#endif
