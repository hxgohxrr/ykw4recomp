# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors




set(dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-deploy")
set(script "${CMAKE_CURRENT_LIST_DIR}/../cmake/deploy.cmake")
file(REMOVE_RECURSE "${dir}")
file(WRITE "${dir}/bin/YKW4Recomp" "runtime")

function(game games)
    file(WRITE "${games}/Game/Game_code.so" "")
endfunction()

function(deploy games)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=XDG_DATA_HOME --unset=HOME ${ARGN}
                            "${CMAKE_COMMAND}" "-DRUNTIME=${dir}/bin/YKW4Recomp" "-DGAMES=${games}" -DWINDOWS=0 -DMACOS=0
                            -P "${script}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
    set(result "${result}" PARENT_SCOPE)
    set(out "${out}" PARENT_SCOPE)
endfunction()
function(deployed games what)
    if(NOT result EQUAL 0 OR NOT EXISTS "${games}/Game/Game")
        file(REMOVE_RECURSE "${dir}")
        message(FATAL_ERROR "${what}: the runtime is not in ${games}/Game/ (${result}):\n${out}")
    endif()
endfunction()

game("${dir}/named")
deploy("${dir}/named")
deployed("${dir}/named" "SNX_GAMES_DIR")
game("${dir}/xdg/YKW4Recomp/games")
deploy("" "XDG_DATA_HOME=${dir}/xdg" "HOME=${dir}/home")
deployed("${dir}/xdg/YKW4Recomp/games" "the user's data folder")
game("${dir}/home/.local/share/YKW4Recomp/games")
deploy("" "XDG_DATA_HOME=relative" "HOME=${dir}/home")
deployed("${dir}/home/.local/share/YKW4Recomp/games" "a relative XDG_DATA_HOME")
game("${dir}/bin/.YKW4Recomp/games")
deploy("" "XDG_DATA_HOME=${dir}/xdg")
deployed("${dir}/bin/.YKW4Recomp/games" "a portable copy's")
file(REMOVE_RECURSE "${dir}/bin/.YKW4Recomp")
deploy("")
file(REMOVE_RECURSE "${dir}")
if(NOT result EQUAL 0 OR NOT out MATCHES "Runtime not deployed: no home folder to find the games in")
    message(FATAL_ERROR "without a home folder deploy.cmake ended with ${result}, not saying so:\n${out}")
endif()
