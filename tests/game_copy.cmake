# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors



set(dir "${CMAKE_CURRENT_BINARY_DIR}/game-copy")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(COPY_FILE "${RUNTIME}" "${dir}/Catherine")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY SDL_VIDEO_DRIVER=dummy
                        "${dir}/Catherine"
                WORKING_DIRECTORY "${dir}" TIMEOUT 20 RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(result EQUAL 0 OR NOT result MATCHES "^[0-9]+$" OR NOT out MATCHES "Cannot find the recompiled code")
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "a game's copy without its code ended with ${result}, not its error:\n${out}")
endif()

file(MAKE_DIRECTORY "${dir}/games/0100ABCD12345000")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY SDL_VIDEO_DRIVER=dummy
                        "${dir}/Catherine" --game=games/0100ABCD12345000/
                WORKING_DIRECTORY "${dir}" TIMEOUT 20 RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
file(REMOVE_RECURSE "${dir}")
if(result EQUAL 0 OR NOT out MATCHES "Cannot find the recompiled code: games/0100ABCD12345000/0100ABCD12345000_code")
    message(FATAL_ERROR "--game= naming a game's folder ended with ${result}, not looking in it:\n${out}")
endif()
