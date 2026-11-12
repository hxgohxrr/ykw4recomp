# Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24).
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors










cmake_minimum_required(VERSION 3.25)



if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER clang)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER clang++)
endif()
project(YKW4Recomp LANGUAGES C CXX)
option(SNX_EXTRACTED_ONLY "Accept extracted game files only; omit container extraction" ON)
if(NOT SNX_EXTRACTED_ONLY)
    message(FATAL_ERROR "This source distribution supports extracted files only. Container extraction sources are not included.")
endif()
option(SNX_YKW4_DISTRIBUTABLE "Use public launcher resources and no game-specific patches" ON)
if(NOT SNX_YKW4_DISTRIBUTABLE)
    message(FATAL_ERROR "The public launcher must use the distributable configuration.")
endif()

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    message(FATAL_ERROR "YKW4Recomp builds with Clang only, and this is ${CMAKE_C_COMPILER_ID} "
                        "(${CMAKE_C_COMPILER}). Use a preset, or -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ "
                        "in a new build folder.")
endif()


if(APPLE AND (NOT DEFINED SNX_BUILD_GUI OR SNX_BUILD_GUI))
    enable_language(OBJC)
    enable_language(Swift)
endif()


set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)



if(APPLE)
    set(SNX_C90_STANDARD 11)
else()
    set(SNX_C90_STANDARD 90)
endif()

option(SNX_BUILD_NX "Build the Switch Horizon/Maxwell runtime" ON)
option(SNX_BUILD_GNUABI "Build the CPU-only Linux AArch64 runtime" ON)
option(SNX_BUILD_3DS "Build 3DS extraction/memory support" ON)

if(SNX_BUILD_NX AND DEFINED SNX_BACKEND AND NOT SNX_BACKEND STREQUAL "sdl3")
    message(FATAL_ERROR "SNX_BACKEND=${SNX_BACKEND}: SDL3 is now mandatory for the NX runtime. "
                        "Configure a new build folder, or remove SNX_BACKEND with -USNX_BACKEND. "
                        "Use -H/--headless at runtime to run without a window.")
endif()
option(SNX_DEPLOY "Copy the runtime next to every recompiled game in SNX_GAMES_DIR after building" ON)
set(SNX_GAMES_DIR "" CACHE PATH "Folder holding the recompiled games (empty: games/ of the drivers' data folder)")

if(SNX_GAMES_DIR STREQUAL "${CMAKE_SOURCE_DIR}/games")
    message(STATUS "SNX_GAMES_DIR was the checkout's games/, an older version's default: the data folder's from now on")
    set_property(CACHE SNX_GAMES_DIR PROPERTY VALUE "")
endif()



if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

option(SNX_LTO "Optimize project targets across translation units" OFF)
if(SNX_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT snx_ipo_supported OUTPUT snx_ipo_error LANGUAGES C CXX)
    if(NOT snx_ipo_supported)
        message(FATAL_ERROR "SNX_LTO requested but unsupported: ${snx_ipo_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
option(SNX_TOOLS_ONLY "Build the C90 extraction/recompilation driver without the runtime" OFF)
option(SNX_HACTOOL_ONLY "Build only the native container extractor" OFF)
set(snx_gui_default OFF)
if(SNX_BUILD_NX AND NOT SNX_TOOLS_ONLY AND NOT SNX_HACTOOL_ONLY)
    set(snx_gui_default ON)
endif()
option(SNX_BUILD_GUI "Build the keyboard-driven SDL3 launcher" ${snx_gui_default})
if(SNX_BUILD_GUI AND (SNX_TOOLS_ONLY OR SNX_HACTOOL_ONLY))
    message(FATAL_ERROR "SNX_BUILD_GUI requires a runtime build, not a tools-only build")
endif()

include(cmake/dependencies.cmake)
if(SNX_HACTOOL_ONLY)
    return()
endif()


include_directories("${PROJECT_SOURCE_DIR}/src")
add_subdirectory(src/common)
if(SNX_BUILD_NX)
    add_subdirectory(src/nx/runtime/driver host-tools)
endif()
include(CTest)
include(cmake/build_info.cmake)  
if(SNX_BUILD_3DS)
    add_subdirectory(src/3ds)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(SNX_TOOLS_ONLY AND SNX_BUILD_NX)
    include(GNUInstallDirs)
    install(TARGETS YKW4Recomp RUNTIME DESTINATION bin)
    install(FILES LICENSE LICENSE.apache NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/YKW4Recomp)
    return()
endif()
if(SNX_BUILD_GNUABI)
    add_subdirectory(src/gnuabi)
endif()
if(SNX_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(NOT WIN32)
    include(GNUInstallDirs)
    install(FILES dist/wimsy.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT desktop)
    install(DIRECTORY dist/icons/hicolor DESTINATION ${CMAKE_INSTALL_DATADIR}/icons COMPONENT desktop)
endif()
if(NOT SNX_BUILD_NX)
    include(GNUInstallDirs)
    install(FILES LICENSE LICENSE.apache NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/YKW4Recomp)
    return()
endif()
if(BUILD_TESTING AND TARGET hactool_lib)
    add_executable(snx-test-hactool-decrypted tests/hactool_decrypted.c)
    snx_test_target(snx-test-hactool-decrypted 11)
    target_link_libraries(snx-test-hactool-decrypted PRIVATE hactool_lib)
    add_test(NAME hactool_decrypted COMMAND snx-test-hactool-decrypted)

    add_executable(snx-test-hactool-layers tests/hactool_layers.c)
    snx_test_target(snx-test-hactool-layers 11)
    target_link_libraries(snx-test-hactool-layers PRIVATE hactool_lib)
    add_test(NAME hactool_layers COMMAND snx-test-hactool-layers $<TARGET_FILE:hactool>)
    snx_sanitizer_path()  
endif()
add_subdirectory(src/nx)
add_subdirectory(tools)

# `cmake --install` puts the runtime, hactool, the desktop entry and the licenses under CMAKE_INSTALL_PREFIX, which is

include(GNUInstallDirs)
if(NOT ANDROID AND NOT IOS)
    install(FILES scripts/mobile_builder.py DESTINATION ${CMAKE_INSTALL_BINDIR}/tools)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        install(FILES scripts/desktop_launcher.py scripts/launcher_core.py scripts/discord_linux.py
                      scripts/switchprep/switchprep.py dist/icons/launcher-logo.png DESTINATION ${CMAKE_INSTALL_BINDIR}/tools)
        install(DIRECTORY src/nx/runtime/langs/ DESTINATION ${CMAKE_INSTALL_BINDIR}/tools/langs FILES_MATCHING PATTERN "*.json")
    endif()
endif()
if(IOS)  
    install(TARGETS YKW4Recomp BUNDLE DESTINATION .)
elseif(NOT ANDROID)  
    install(TARGETS YKW4Recomp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET hactool AND ANDROID)
    install(TARGETS hactool RUNTIME DESTINATION lib/${ANDROID_ABI})
elseif(TARGET hactool)
    install(TARGETS hactool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(NOT WIN32)
    install(FILES dist/YKW4Recomp.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT desktop)
endif()
if(ANDROID)  
    install(FILES dist/icons/hicolor/256x256/apps/wimsy.png DESTINATION res/mipmap-xxxhdpi RENAME ic_launcher.png)
endif()

# The licenses go with the binaries: the fork's (GPL-3.0), MZ Labs' (Apache 2.0, with its NOTICE), and those of the

set(licenses "${CMAKE_INSTALL_DATADIR}/licenses/YKW4Recomp")
install(FILES LICENSE LICENSE.apache NOTICE DESTINATION ${licenses})
install(FILES src/nx/runtime/vendor/miniz/LICENSE DESTINATION ${licenses}/miniz)
install(FILES src/nx/runtime/vendor/cjson/LICENSE DESTINATION ${licenses}/cJSON)

install(FILES "${FFmpeg_SOURCE_DIR}/COPYING.LGPLv2.1" "${FFmpeg_SOURCE_DIR}/LICENSE.md" cmake/ffmpeg-vp9-nvdec.patch
        DESTINATION ${licenses}/FFmpeg)
install(FILES LICENSE.apache DESTINATION ${licenses}/OpenSans RENAME LICENSE)  
install(FILES "${Vulkan-Headers_SOURCE_DIR}/LICENSE.md" DESTINATION ${licenses}/Vulkan-Headers)
install(DIRECTORY "${Vulkan-Headers_SOURCE_DIR}/LICENSES" DESTINATION ${licenses}/Vulkan-Headers)
install(FILES "${volk_SOURCE_DIR}/LICENSE.md" DESTINATION ${licenses}/volk)
install(FILES "${VulkanMemoryAllocator_SOURCE_DIR}/LICENSE.txt" DESTINATION ${licenses}/VulkanMemoryAllocator)
install(FILES "${OpenSSL_SOURCE_DIR}/LICENSE.txt" DESTINATION ${licenses}/OpenSSL)
# lib/LICENSE is the library's BSD-2-Clause. The top-level LICENSE covers the rest of LZ4 (GPL-2.0-or-later), none

install(FILES "${lz4_SOURCE_DIR}/lib/LICENSE" DESTINATION ${licenses}/lz4)
if(SNX_SDL3_FROM_SOURCE)
    install(FILES "${SDL3_SOURCE_DIR}/LICENSE.txt" DESTINATION ${licenses}/SDL3)
endif()
if(TARGET hactool)
    install(FILES "${hactool_SOURCE_DIR}/LICENSE" DESTINATION ${licenses}/hactool)
    # cJSON's license is the comment at the top of its header
    file(READ "${hactool_SOURCE_DIR}/cJSON.h" cjson)
    string(FIND "${cjson}" "*/" cjson_end)
    string(SUBSTRING "${cjson}" 0 ${cjson_end} cjson)
    file(WRITE "${CMAKE_BINARY_DIR}/cJSON-LICENSE.txt" "${cjson}*/\n")
    install(FILES "${CMAKE_BINARY_DIR}/cJSON-LICENSE.txt" DESTINATION ${licenses}/hactool/cJSON RENAME LICENSE.txt)
endif()
