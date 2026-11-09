# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors


set(snx_cpu "${PROJECT_SOURCE_DIR}/src/common/cpu/aarch64")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/common/cpu/arm" arm-backend)
file(GLOB snx_rtlib_sources CONFIGURE_DEPENDS "${snx_cpu}/*.h" "${snx_cpu}/*.c" "${snx_cpu}/*.inc"
    "${snx_cpu}/helpers/*.c" "${PROJECT_SOURCE_DIR}/src/common/runtime/interface.h")
if(CMAKE_CROSSCOMPILING)

    find_program(SNX_HOST_CC NAMES clang cc REQUIRED)
    set(snx_rtlib_gen "${CMAKE_CURRENT_BINARY_DIR}/snx-rtlib-gen-host")
    add_custom_command(OUTPUT "${snx_rtlib_gen}"
        COMMAND "${SNX_HOST_CC}" -O1 -w -I "${PROJECT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/rtlib_gen.c" "${snx_cpu}/a64.c"
            -o "${snx_rtlib_gen}"
        DEPENDS rtlib_gen.c ${snx_rtlib_sources} VERBATIM)
else()
    add_executable(snx-rtlib-gen rtlib_gen.c)
    target_link_libraries(snx-rtlib-gen PRIVATE YKW4Recomp-common)
    set(snx_rtlib_gen snx-rtlib-gen)
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc"
    COMMAND "${snx_rtlib_gen}" interp "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc"
    DEPENDS "${snx_rtlib_gen}" VERBATIM)





if(ANDROID)
    set(snx_rtlib_flags --target=${CMAKE_C_COMPILER_TARGET} --sysroot=${CMAKE_SYSROOT})
elseif(APPLE)  


    set(snx_rtlib_flags ${SNX_APPLE_FLAGS} -D_FORTIFY_SOURCE=0)
else()
    set(snx_rtlib_flags "")
endif()
list(APPEND snx_rtlib_flags -O2 -fvisibility=hidden -fno-strict-aliasing -fwrapv -w -fno-semantic-interposition
    -foptimize-sibling-calls -fno-stack-protector)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    list(APPEND snx_rtlib_flags -mcx16)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")

    list(APPEND snx_rtlib_flags -mno-outline-atomics)
endif()
if(WIN32)
    list(APPEND snx_rtlib_flags -mno-stack-arg-probe)
else()
    list(APPEND snx_rtlib_flags -fPIC)
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib${CMAKE_C_OUTPUT_EXTENSION}"
    COMMAND "${CMAKE_C_COMPILER}" ${snx_rtlib_flags} -I "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${PROJECT_SOURCE_DIR}/src" -c "${CMAKE_CURRENT_SOURCE_DIR}/rtlib.c"
        -o "${CMAKE_CURRENT_BINARY_DIR}/rtlib${CMAKE_C_OUTPUT_EXTENSION}"
    DEPENDS rtlib.c libc.inc "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc" ${snx_rtlib_sources} VERBATIM)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib_object.c"
    COMMAND "${snx_rtlib_gen}" embed "${CMAKE_CURRENT_BINARY_DIR}/rtlib${CMAKE_C_OUTPUT_EXTENSION}"
        "${CMAKE_CURRENT_BINARY_DIR}/rtlib_object.c" snx_rtlib_object
    DEPENDS "${snx_rtlib_gen}" "${CMAKE_CURRENT_BINARY_DIR}/rtlib${CMAKE_C_OUTPUT_EXTENSION}" VERBATIM)

string(REGEX MATCH "^[0-9]+" snx_clang_major "${CMAKE_C_COMPILER_VERSION}")
string(REGEX MATCH "^[0-9]+" snx_llvm_major "${SNX_LLVM_VERSION}")
if(snx_clang_major GREATER snx_llvm_major)




    if(SNX_LLVM_CONFIG)
        snx_llvm_query(SNX_LLVM_BINDIR --bindir)
    else()
        set(SNX_LLVM_BINDIR "${LLVM_TOOLS_BINARY_DIR}")
    endif()
    find_program(SNX_LLVM_AS NAMES llvm-as llvm-as-${snx_llvm_major} HINTS "${SNX_LLVM_BINDIR}" NO_DEFAULT_PATH)
    if(NOT SNX_LLVM_AS)
        message(FATAL_ERROR "Clang ${CMAKE_C_COMPILER_VERSION} writes the runtime library's bitcode, which LLVM "
                            "${SNX_LLVM_VERSION} (SNX_LLVM_CONFIG) cannot read, and that LLVM ships no llvm-as to "
                            "translate. Use a Clang no newer than LLVM ${snx_llvm_major}, or an LLVM as new as this "
                            "Clang.")
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir_raw.ll"
        COMMAND "${CMAKE_C_COMPILER}" ${snx_rtlib_flags} -g0 -emit-llvm -S -I "${CMAKE_CURRENT_BINARY_DIR}"
            -I "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/rtlib.c"
            -o "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir_raw.ll"
        DEPENDS rtlib.c "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc" ${snx_rtlib_sources} VERBATIM)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir.ll"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir_raw.ll
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir.ll
            -P "${CMAKE_CURRENT_SOURCE_DIR}/rtlib_ir_compat.cmake"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir_raw.ll" rtlib_ir_compat.cmake VERBATIM)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
        COMMAND "${SNX_LLVM_AS}" "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir.ll" -o "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rtlib_ir.ll" VERBATIM)
else()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
        COMMAND "${CMAKE_C_COMPILER}" ${snx_rtlib_flags} -emit-llvm -I "${CMAKE_CURRENT_BINARY_DIR}"
            -I "${PROJECT_SOURCE_DIR}/src" -c "${CMAKE_CURRENT_SOURCE_DIR}/rtlib.c"
            -o "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
        DEPENDS rtlib.c "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc" ${snx_rtlib_sources} VERBATIM)
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
    COMMAND "${CMAKE_C_COMPILER}" ${snx_rtlib_flags} -emit-llvm -I "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${PROJECT_SOURCE_DIR}/src" -c "${CMAKE_CURRENT_SOURCE_DIR}/rtlib.c"
        -o "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
    DEPENDS rtlib.c libc.inc "${CMAKE_CURRENT_BINARY_DIR}/rtlib_interp.inc" ${snx_rtlib_sources} VERBATIM)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rtlib_bitcode.c"
    COMMAND "${snx_rtlib_gen}" embed "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc"
        "${CMAKE_CURRENT_BINARY_DIR}/rtlib_bitcode.c" snx_rtlib_bitcode
    DEPENDS "${snx_rtlib_gen}" "${CMAKE_CURRENT_BINARY_DIR}/rtlib.bc" VERBATIM)


file(GLOB snx_armlib_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/common/cpu/arm/*.h"
    "${PROJECT_SOURCE_DIR}/src/common/cpu/arm/*_helpers.c")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/armlib.bc"
    COMMAND "${CMAKE_C_COMPILER}" ${snx_rtlib_flags} -emit-llvm -I "${PROJECT_SOURCE_DIR}/src"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/armlib.c" -o "${CMAKE_CURRENT_BINARY_DIR}/armlib.bc"
    DEPENDS armlib.c libc.inc ${snx_armlib_sources} ${snx_rtlib_sources} VERBATIM)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/armlib_bitcode.c"
    COMMAND "${snx_rtlib_gen}" embed "${CMAKE_CURRENT_BINARY_DIR}/armlib.bc"
        "${CMAKE_CURRENT_BINARY_DIR}/armlib_bitcode.c" snx_armlib_bitcode
    DEPENDS "${snx_rtlib_gen}" "${CMAKE_CURRENT_BINARY_DIR}/armlib.bc" VERBATIM)

add_library(snx_llvm STATIC ir.c ir_a64.c ir_simd.c ir_fp.c ir_codegen.c link.c link_elf.c link_coff.c
    "${CMAKE_CURRENT_BINARY_DIR}/rtlib_object.c" "${CMAKE_CURRENT_BINARY_DIR}/rtlib_bitcode.c"
    "${CMAKE_CURRENT_BINARY_DIR}/armlib_bitcode.c")
set_target_properties(snx_llvm PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
target_include_directories(snx_llvm SYSTEM PUBLIC "${SNX_LLVM_INCLUDE_DIR}")
target_compile_options(snx_llvm PRIVATE -Wall -Wextra -Werror=missing-prototypes)
target_compile_definitions(snx_llvm PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_directories(snx_llvm PUBLIC "${SNX_LLVM_LIBRARY_DIR}")
target_link_libraries(snx_llvm PUBLIC YKW4Recomp-common snx_arm_llvm ${SNX_LLVM_LIBS} ${SNX_LLVM_SYSTEM_LIBS} Threads::Threads)
