# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors


include_guard(GLOBAL)
find_program(SNX_HOST_CC NAMES clang cc)
if(NOT SNX_HOST_CC)
    message(FATAL_ERROR "A cross build builds programs for this machine too (FFmpeg's helpers, swipc), which needs a C "
                        "compiler for it (clang or cc). Install one or set SNX_HOST_CC to its path.")
endif()
