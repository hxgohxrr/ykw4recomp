/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nx/runtime/driver/extract.h"
int snx_extract(int argc, char** argv, const SnxPaths* paths) {
    (void)argc; (void)argv; (void)paths;
    return snx_error("This build accepts extracted ExeFS/RomFS only. Container extraction and decryption are not included.");
}
