/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_LANG_H
#define YKW4_LAUNCHER_LANG_H
#include <windows.h>
const wchar_t* launcher_lang_id(void);
int launcher_lang_init(const wchar_t* base, const wchar_t* ini);
void launcher_lang_combo(HWND combo);
int launcher_lang_choose(HWND window);
int launcher_lang_save(const wchar_t* ini);
const wchar_t* launcher_text(const wchar_t* source);
void launcher_lang_apply(HWND window);
BOOL launcher_set_control_text(HWND window, int id, const wchar_t* text);
int launcher_message(HWND window, const wchar_t* text, const wchar_t* caption, UINT type);
#ifndef LAUNCHER_LANG_IMPLEMENTATION
#define SetDlgItemTextW launcher_set_control_text
#define MessageBoxW launcher_message
#endif
#endif
