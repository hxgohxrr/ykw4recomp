# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors






foreach(var FROM TO KEY)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "deps_publish.cmake needs -D${var}=...")
    endif()
endforeach()


if(EXISTS "${TO}")
    return()
endif()
string(RANDOM LENGTH 8 tag)
set(partial "${TO}.partial-${tag}")
file(COPY "${FROM}/" DESTINATION "${partial}")
file(COPY_FILE "${KEY}" "${partial}/snx-complete")

file(RENAME "${partial}" "${TO}" RESULT failed)
if(failed)
    file(REMOVE_RECURSE "${partial}")
endif()
