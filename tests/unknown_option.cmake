# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors


set(dir "${CMAKE_CURRENT_BINARY_DIR}/unknown-option")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
foreach(option --resoltion=2 --resolution=2 --seconds=5)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY SDL_VIDEO_DRIVER=dummy
                            "${RUNTIME}" ${option} missing.nro
                    WORKING_DIRECTORY "${dir}" TIMEOUT 20 RESULT_VARIABLE result OUTPUT_VARIABLE out
                    ERROR_VARIABLE out)
    if(option STREQUAL "--resoltion=2")
        set(expected "unknown option: --resoltion=2")
    else()
        set(expected "Input dump is missing")
    endif()
    string(FIND "${out}" "${expected}" at)
    if(result EQUAL 0 OR at LESS 0)
        file(REMOVE_RECURSE "${dir}")
        message(FATAL_ERROR "YKW4Recomp ${option} missing.nro ended with ${result}, not \"${expected}\":\n${out}")
    endif()
endforeach()
file(REMOVE_RECURSE "${dir}")
