# Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24).
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors










set(CPM_VERSION 0.43.2)
set(CPM_HASH 49a3bef91ceb65bb66d57255e12d1ffd22abc2f6408fa9fe4534c544a2f232aa)
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/cpm" CACHE PATH "Where CPM.cmake keeps downloaded packages")
endif()
if(DEFINED CPM_SOURCE_CACHE)
    set(cpm_file "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_VERSION}.cmake")
else()
    set(cpm_file "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_VERSION}.cmake")
endif()


set(cpm_have "")
if(EXISTS "${cpm_file}")
    file(SHA256 "${cpm_file}" cpm_have)
endif()
if(NOT cpm_have STREQUAL CPM_HASH)
    file(DOWNLOAD "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_VERSION}/CPM.cmake"
         "${cpm_file}.part" EXPECTED_HASH SHA256=${CPM_HASH})
    file(RENAME "${cpm_file}.part" "${cpm_file}")
endif()
include("${cpm_file}")





function(snx_quiet)
    foreach(target IN LISTS ARGN)
        target_compile_options(${target} PRIVATE -w)
    endforeach()
endfunction()




if(SNX_BUILD_NX OR SNX_HACTOOL_ONLY)
    CPMAddPackage(NAME lz4 VERSION 1.10.0
                  URL https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz
                  URL_HASH SHA256=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
                  DOWNLOAD_ONLY YES)
    add_library(snx_lz4 STATIC "${lz4_SOURCE_DIR}/lib/lz4.c")
    target_include_directories(snx_lz4 PUBLIC "${lz4_SOURCE_DIR}/lib")
    snx_quiet(snx_lz4)
endif()




include("${CMAKE_SOURCE_DIR}/cmake/deps_cache.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/apple.cmake")
if(SNX_BUILD_NX OR SNX_BUILD_3DS OR SNX_HACTOOL_ONLY)
    set(snx_openssl_sha256 325b5c806167c13b40b1ffeadfe0248197c00eccc4cf123ec1e28d2d2fd216d9)
    CPMAddPackage(NAME OpenSSL VERSION 4.0.3
                  URL https://github.com/openssl/openssl/releases/download/openssl-4.0.3/openssl-4.0.3.tar.gz
                  URL_HASH SHA256=${snx_openssl_sha256}
                  DOWNLOAD_ONLY YES)
    include("${CMAKE_SOURCE_DIR}/cmake/openssl.cmake")
endif()

if((SNX_BUILD_NX AND NOT SNX_TOOLS_ONLY AND NOT SNX_HACTOOL_ONLY) OR SNX_BUILD_3DS)
    CPMAddPackage(NAME Vulkan-Headers VERSION 1.4.365
                  URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.365.tar.gz
                  URL_HASH SHA256=ed832b3292cce324ebba6522fd2d3004bf1b797fa49bfbcad0b302ff23d56abc
                  EXCLUDE_FROM_ALL YES)

    CPMAddPackage(NAME volk VERSION 1.4.363
                  URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
                  URL_HASH SHA256=1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3
                  DOWNLOAD_ONLY YES)
endif()


if(SNX_BUILD_NX AND NOT SNX_TOOLS_ONLY AND NOT SNX_HACTOOL_ONLY)

    set(snx_ffmpeg_sha256 8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e)
    CPMAddPackage(NAME FFmpeg VERSION 9.0.2
                  URL https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz
                  URL_HASH SHA256=${snx_ffmpeg_sha256}
                  DOWNLOAD_ONLY YES)
    include("${CMAKE_SOURCE_DIR}/cmake/ffmpeg.cmake")




    CPMAddPackage(NAME OpenSans VERSION 1.101
                  URL https://raw.githubusercontent.com/google/fonts/4e24bf1805f1526cd1ce3e39447d5f8e6f1f6b67/apache/opensans/OpenSans-Regular.ttf
                  URL_HASH SHA256=13c03e22a633919beb2847c58c8285fb8a735ee97097d7c48fd403f8294b05f8
                  DOWNLOAD_ONLY YES DOWNLOAD_NO_EXTRACT YES)
    CPMAddPackage(NAME YuzuOSSExtension VERSION 2019
                  URL https://raw.githubusercontent.com/eden-emulator/mirror/2b87305d319f0bd04da36fb96314934f37b12637/src/core/file_sys/system_archive/data/font_nintendo_extended.cpp
                  URL_HASH SHA256=6fbbf5176ff4fd2024f9d150c889477f6b45ece4ac5e45d51286e32f8b13a6f2
                  DOWNLOAD_ONLY YES DOWNLOAD_NO_EXTRACT YES)
    include("${CMAKE_SOURCE_DIR}/cmake/shared_fonts.cmake")

    CPMAddPackage(NAME VulkanMemoryAllocator VERSION 3.4.0
                  URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz
                  URL_HASH SHA256=822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1
                  DOWNLOAD_ONLY YES)
endif()

set(SNX_SDL3_FROM_SOURCE OFF)
if((SNX_BUILD_NX AND NOT SNX_TOOLS_ONLY AND NOT SNX_HACTOOL_ONLY) OR SNX_BUILD_GUI OR SNX_BUILD_3DS)

    CPMFindPackage(NAME SDL3 VERSION 3.4.18 FIND_PACKAGE_ARGUMENTS CONFIG
                   URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz
                   URL_HASH SHA256=c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90
                   EXCLUDE_FROM_ALL YES SYSTEM YES
                   OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_TESTS OFF"
                           "SDL_EXAMPLES OFF" "SDL_INSTALL OFF" "SDL_UNIX_CONSOLE_BUILD ON")
    if(SDL3_ADDED)
        set(SNX_SDL3_FROM_SOURCE ON)
        snx_quiet(SDL3-static)

        if(CMAKE_C_COMPILER_LAUNCHER)
            set_target_properties(SDL3-static PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
        endif()
    endif()
endif()


# stroker come from FreeType, under the FreeType License, source/FTL.TXT). cmake/splash.cmake builds both
if(SNX_BUILD_GUI)
    CPMAddPackage(NAME plutovg VERSION 1.3.3
                  URL https://github.com/sammycage/plutovg/archive/refs/tags/v1.3.3.tar.gz
                  URL_HASH SHA256=2b0d17a6e016f47b86f9c00e2cb82600041b1ea1f7d2a00c2d46ae542cbfed3c
                  DOWNLOAD_ONLY YES)
    CPMAddPackage(NAME lunasvg VERSION 3.5.0
                  URL https://github.com/sammycage/lunasvg/archive/refs/tags/v3.5.0.tar.gz
                  URL_HASH SHA256=1abf1472ee6c4d19797916e8cc3c2e4b628e0d81178ffac60bdb0d457e32c690
                  DOWNLOAD_ONLY YES)
endif()




if(NOT SNX_EXTRACTED_ONLY AND (SNX_BUILD_NX OR SNX_HACTOOL_ONLY) AND NOT SNX_TOOLS_ONLY AND (NOT CMAKE_CROSSCOMPILING OR ANDROID))
    set(hactool_patch "${CMAKE_SOURCE_DIR}/cmake/hactool-decrypted.patch")
    set(hactool_openssl_patch "${CMAKE_SOURCE_DIR}/cmake/hactool-openssl.patch")
    set(hactool_libhac_patch "${CMAKE_SOURCE_DIR}/cmake/hactool-libhac.patch")
    CPMAddPackage(NAME hactool VERSION 1.4.0.13
                  URL https://github.com/SciresM/hactool/archive/1d64a83450e025622f3468c28fc4164dad2c5ef6.tar.gz
                  URL_HASH SHA256=f7148daad624ba909f5ae253210734b1a0ce19e89a7914b6cf71401f7134ebbc
                  DOWNLOAD_ONLY YES)
    include("${CMAKE_SOURCE_DIR}/cmake/hactool.cmake")
endif()
