/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_MODS_H
#define YKW4_LAUNCHER_MODS_H
#include <windows.h>
typedef struct LauncherMods LauncherMods;
LauncherMods* launcher_mods_load(HWND window, const wchar_t* base, const wchar_t* ini);
int launcher_mods_command(HWND window, LauncherMods* mods, int id);
int launcher_mods_write_ini(HWND window, LauncherMods* mods, const wchar_t* ini);
int launcher_mods_publish(LauncherMods* mods);
void launcher_mods_free(LauncherMods* mods);
int launcher_mods_import_zip(HWND window, LauncherMods* mods, const wchar_t* archive, const wchar_t* name, int translation, const wchar_t* source);
void launcher_mods_retranslate(HWND window, LauncherMods* mods);
void launcher_mods_rescan(HWND window, LauncherMods* mods);
int launcher_mods_has_spanish(const LauncherMods* mods);
void launcher_mods_select_spanish(HWND window, LauncherMods* mods, int enabled);
void launcher_mods_spanish(HWND window, LauncherMods* mods);
#endif
