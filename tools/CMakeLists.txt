# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors

add_executable(snx-shaderc snx_shaderc.c)
target_link_libraries(snx-shaderc PRIVATE snx_shader)
if(NOT WIN32)
    target_link_libraries(snx-shaderc PRIVATE Threads::Threads m)
endif()
set_target_properties(snx-shaderc PROPERTIES LINKER_LANGUAGE CXX RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
