# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors


include_guard(GLOBAL)
add_library(snx_pad_sdl STATIC "${PROJECT_SOURCE_DIR}/src/common/utilities/pad_sdl.c")
target_link_libraries(snx_pad_sdl PUBLIC YKW4Recomp-common SDL3::SDL3)
target_compile_options(snx_pad_sdl PRIVATE -Wall -Wextra -Werror=missing-prototypes)
