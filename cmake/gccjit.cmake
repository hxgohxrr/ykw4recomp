# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors



option(SNX_GCCJIT "Build the recompiler's libgccjit backend when libgccjit is found" ON)
set(SNX_GCCJIT_ROOT "" CACHE PATH "A prefix holding lib/gcc/<triple>/<version>/{include/libgccjit.h,libgccjit.so}")
set(SNX_GCCJIT_FOUND OFF)
if(SNX_GCCJIT)
    set(snx_gccjit_hints)
    foreach(prefix "${SNX_GCCJIT_ROOT}" "${SNX_GCCJIT_ROOT}/usr" /usr /usr/local)
        if(prefix)
            file(GLOB snx_gcc_dirs "${prefix}/lib/gcc/*/*")
            list(APPEND snx_gccjit_hints ${snx_gcc_dirs})
        endif()
    endforeach()
    list(SORT snx_gccjit_hints COMPARE NATURAL ORDER DESCENDING)  
    find_path(SNX_GCCJIT_INCLUDE_DIR libgccjit.h HINTS ${snx_gccjit_hints} PATH_SUFFIXES include)
    find_library(SNX_GCCJIT_LIBRARY gccjit HINTS ${snx_gccjit_hints})
    if(SNX_GCCJIT_INCLUDE_DIR AND SNX_GCCJIT_LIBRARY)
        set(SNX_GCCJIT_FOUND ON)
        message(STATUS "libgccjit backend: ${SNX_GCCJIT_LIBRARY}")
    else()
        message(STATUS "libgccjit backend: libgccjit not found, set SNX_GCCJIT_ROOT to build it")
    endif()
endif()
