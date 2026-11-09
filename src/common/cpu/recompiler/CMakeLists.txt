# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors






option(SNX_RECOMPILER_SHARED "Also build the recompiler as a shared library (libYKW4Recomp-recompiler.so)" OFF)


option(SNX_RECOMPILER_INSTALL "Install the recompiler library, its headers and its CMake package" ${SNX_TOOLS_ONLY})
include(GNUInstallDirs)
set(snx_recompiler_parts YKW4Recomp-common snx_llvm snx_arm_llvm snx_arm_rt)
set(snx_recompiler_external ${SNX_LLVM_LIBS} ${SNX_LLVM_SYSTEM_LIBS} Threads::Threads ${CMAKE_DL_LIBS})
set(snx_recompiler_defines _CRT_SECURE_NO_WARNINGS)
if(TARGET snx_jit)
    list(APPEND snx_recompiler_parts snx_jit)
    list(APPEND snx_recompiler_external "${SNX_GCCJIT_LIBRARY}")
    list(APPEND snx_recompiler_defines SNX_WITH_GCCJIT)
endif()
if(WIN32)
    list(APPEND snx_recompiler_external advapi32)
else()
    list(APPEND snx_recompiler_external m)
endif()
set(snx_recompiler_objects)
foreach(part IN LISTS snx_recompiler_parts)
    list(APPEND snx_recompiler_objects "$<TARGET_OBJECTS:${part}>")
endforeach()
if(SNX_RECOMPILER_SHARED)
    set_target_properties(${snx_recompiler_parts} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(snx_recompiler_kinds STATIC SHARED)
else()
    set(snx_recompiler_kinds STATIC)
endif()
foreach(kind IN LISTS snx_recompiler_kinds)
    if(kind STREQUAL STATIC)
        set(target YKW4Recomp-recompiler)
    else()
        set(target YKW4Recomp-recompiler-shared)
    endif()
    add_library(${target} ${kind} library.c ${snx_recompiler_objects})
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON
        OUTPUT_NAME YKW4Recomp-recompiler)
    add_dependencies(${target} ${snx_recompiler_parts})
    target_include_directories(${target} PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
                                                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/YKW4Recomp>")

    foreach(part IN LISTS snx_recompiler_parts)
        target_include_directories(${target} PRIVATE "$<TARGET_PROPERTY:${part},INTERFACE_INCLUDE_DIRECTORIES>")
    endforeach()
    target_include_directories(${target} SYSTEM PRIVATE "${SNX_LLVM_INCLUDE_DIR}")
    if(TARGET snx_jit)
        target_include_directories(${target} SYSTEM PRIVATE "${SNX_GCCJIT_INCLUDE_DIR}")
    endif()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror=missing-prototypes)
    target_compile_definitions(${target} PRIVATE ${snx_recompiler_defines})
    target_link_directories(${target} PUBLIC "${SNX_LLVM_LIBRARY_DIR}")
    if(kind STREQUAL STATIC)
        target_link_libraries(${target} PUBLIC ${snx_recompiler_external})
    else()


        set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden)
        target_compile_definitions(${target} PRIVATE SNX_RECOMPILER_EXPORTS)
        target_link_libraries(${target} PRIVATE ${snx_recompiler_external})
        if(NOT WIN32 AND NOT APPLE)
            target_link_options(${target} PRIVATE "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/snx_recompiler.map")
            set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/snx_recompiler.map")
        endif()
    endif()
endforeach()


if(NOT SNX_RECOMPILER_INSTALL)
    return()
endif()
install(TARGETS YKW4Recomp-recompiler EXPORT YKW4RecompRecompiler ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(SNX_RECOMPILER_SHARED)
    install(TARGETS YKW4Recomp-recompiler-shared EXPORT YKW4RecompRecompiler
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(EXPORT YKW4RecompRecompiler NAMESPACE YKW4Recomp:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YKW4RecompRecompiler
    FILE YKW4RecompRecompilerTargets.cmake)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/YKW4RecompRecompilerConfig.cmake"
    "include(CMakeFindDependencyMacro)\n"
    "set(THREADS_PREFER_PTHREAD_FLAG ON)\n"
    "find_dependency(Threads)\n"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/YKW4RecompRecompilerTargets.cmake\")\n"
    "add_library(YKW4Recomp::recompiler ALIAS YKW4Recomp::YKW4Recomp-recompiler)\n"
    "if(TARGET YKW4Recomp::YKW4Recomp-recompiler-shared)\n"
    "    add_library(YKW4Recomp::recompiler-shared ALIAS YKW4Recomp::YKW4Recomp-recompiler-shared)\n"
    "endif()\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/YKW4RecompRecompilerConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YKW4RecompRecompiler)

set(snx_recompiler_include "${CMAKE_INSTALL_INCLUDEDIR}/YKW4Recomp")
install(FILES snx_recompiler.h DESTINATION ${snx_recompiler_include})
install(FILES "${PROJECT_SOURCE_DIR}/src/common/cpu/aarch64/cpu.h"
        DESTINATION ${snx_recompiler_include}/common/cpu/aarch64)
install(FILES "${PROJECT_SOURCE_DIR}/src/common/cpu/arm/arm.h" DESTINATION ${snx_recompiler_include}/common/cpu/arm)
install(FILES "${PROJECT_SOURCE_DIR}/src/common/runtime/interface.h"
        DESTINATION ${snx_recompiler_include}/common/runtime)
