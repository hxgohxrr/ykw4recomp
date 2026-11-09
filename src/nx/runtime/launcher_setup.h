/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_SETUP_H
#define YKW4_LAUNCHER_SETUP_H
#include <windows.h>
int launcher_setup(const wchar_t* base, const wchar_t* executable);
int launcher_update(const wchar_t* base, const wchar_t* executable);
int launcher_publish_update(const wchar_t* base, const wchar_t* data, const wchar_t* code, const wchar_t* backup);

int launcher_prepare_main(const wchar_t* input, const wchar_t* output);
#endif
