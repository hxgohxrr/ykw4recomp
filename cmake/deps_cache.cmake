# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors









set(SNX_DEPS_CACHE "$ENV{SNX_DEPS_CACHE}" CACHE PATH
    "Folder where FFmpeg and OpenSSL, once built, are kept for every build tree to reuse")


function(snx_deps_headers_hash out)
    set(probe "${CMAKE_BINARY_DIR}/snx-deps/headers.c")
    file(WRITE "${probe}" "#include <errno.h>\n#include <limits.h>\n#include <math.h>\n#include <stddef.h>\n"
                          "#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
                          "#include <time.h>\n#ifdef _WIN32\n#include <windows.h>\n#else\n#include <pthread.h>\n"
                          "#include <unistd.h>\n#endif\n")
    set(flags "")
    if(CMAKE_C_COMPILER_TARGET)
        list(APPEND flags "--target=${CMAKE_C_COMPILER_TARGET}")
    endif()
    if(CMAKE_SYSROOT)
        list(APPEND flags "--sysroot=${CMAKE_SYSROOT}")
    endif()

    execute_process(COMMAND "${CMAKE_C_COMPILER}" ${flags} -E -P "${probe}" OUTPUT_VARIABLE text ERROR_VARIABLE err
                    RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "SNX_DEPS_CACHE: the compiler could not read the system's C headers:\n${err}")
    endif()
    string(SHA256 hash "${text}")
    set(${out} "${hash}" PARENT_SCOPE)
endfunction()





function(snx_deps_cached name out)
    set(${out}_FOUND FALSE PARENT_SCOPE)
    set(${out}_DIR "" PARENT_SCOPE)
    if(NOT SNX_DEPS_CACHE)
        return()
    endif()
    get_property(headers GLOBAL PROPERTY snx_deps_headers)
    if(NOT headers)
        snx_deps_headers_hash(headers)
        set_property(GLOBAL PROPERTY snx_deps_headers "${headers}")
    endif()
    file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" recipe)
    string(JOIN "\n" key "${name}" ${ARGN} "compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
                "headers: ${headers}" "recipe: ${recipe}")
    string(SHA256 hash "${key}")
    string(SUBSTRING "${hash}" 0 16 hash)
    set(dir "${SNX_DEPS_CACHE}/${name}-${hash}")
    file(WRITE "${CMAKE_BINARY_DIR}/snx-deps/${name}.key" "${key}\n")
    file(WRITE "${CMAKE_BINARY_DIR}/snx-deps/${name}.dir" "${name}-${hash}\n")
    set(${out}_DIR "${dir}" PARENT_SCOPE)
    if(EXISTS "${dir}/snx-complete")
        message(STATUS "${name}: the build in ${dir}")
        set(${out}_FOUND TRUE PARENT_SCOPE)
    endif()
endfunction()



function(snx_deps_publish project name prefix dir)
    if(NOT dir)
        return()
    endif()
    ExternalProject_Add_Step(${project} publish
        COMMAND "${CMAKE_COMMAND}" "-DFROM=${prefix}" "-DTO=${dir}" "-DKEY=${CMAKE_BINARY_DIR}/snx-deps/${name}.key"
                -P "${CMAKE_SOURCE_DIR}/cmake/deps_publish.cmake"
        COMMENT "Keeping ${name} in ${dir}"
        DEPENDEES install)
endfunction()
