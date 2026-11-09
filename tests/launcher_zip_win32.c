/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../src/nx/runtime/launcher_zip.c"
int wmain(int argc, wchar_t** argv) {
    wchar_t error[512] = {0};
    if (argc != 4) return 2;
    int result = launcher_zip_extract(argv[1], argv[2], error, 512);
    return result == _wtoi(argv[3]) ? 0 : 1;
}
