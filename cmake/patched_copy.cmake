# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors











function(snx_patched_copy)
    cmake_parse_arguments(arg "" "SOURCE;DEST" "PATCHES;FILES" ${ARGN})
    set(stale FALSE)
    set(pairs ${arg_FILES})
    while(pairs)
        list(POP_FRONT pairs name hash)
        set(have "")
        if(EXISTS "${arg_DEST}/${name}")
            file(SHA256 "${arg_DEST}/${name}" have)
        endif()
        if(NOT have STREQUAL hash)
            set(stale TRUE)
        endif()
    endwhile()
    if(NOT stale)
        return()
    endif()

    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        get_filename_component(git_dir "${GIT_EXECUTABLE}" DIRECTORY)
    endif()
    find_program(PATCH_EXECUTABLE patch HINTS "${git_dir}/../usr/bin" "${git_dir}/../../usr/bin")
    if(NOT PATCH_EXECUTABLE)
        message(FATAL_ERROR "patch(1) is needed to patch ${arg_SOURCE}: install it, or Git for Windows")
    endif()
    file(REMOVE_RECURSE "${arg_DEST}")
    file(COPY "${arg_SOURCE}/" DESTINATION "${arg_DEST}")
    foreach(patch IN LISTS arg_PATCHES)
        execute_process(COMMAND "${PATCH_EXECUTABLE}" -p1 -i "${patch}" WORKING_DIRECTORY "${arg_DEST}"
                        RESULT_VARIABLE failed OUTPUT_VARIABLE said ERROR_VARIABLE said)
        if(failed)
            message(FATAL_ERROR "${patch} does not apply to ${arg_SOURCE}:\n${said}")
        endif()
    endforeach()
    set(pairs ${arg_FILES})
    while(pairs)
        list(POP_FRONT pairs name hash)
        file(SHA256 "${arg_DEST}/${name}" have)
        if(NOT have STREQUAL hash)
            message(FATAL_ERROR "${arg_DEST}/${name} (SHA-256 ${have}) is not what the patches make of ${arg_SOURCE}. "
                                "After a bump of it or a patch edit, check the patched files and update their hashes")
        endif()
    endwhile()
endfunction()
