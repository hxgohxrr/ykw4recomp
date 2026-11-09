/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_LAUNCHER_ZIP_H
#define YKW4_LAUNCHER_ZIP_H
#include <windows.h>
int launcher_zip_extract(const wchar_t* archive, const wchar_t* destination, wchar_t* error, size_t capacity);
#endif
