# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors











set(snx_ffmpeg_src "${CMAKE_BINARY_DIR}/_deps/ffmpeg-patched")
set(snx_ffmpeg_prefix "${CMAKE_BINARY_DIR}/ffmpeg")

if(snx_ffmpeg_src MATCHES "[ \t]")
    message(FATAL_ERROR "FFmpeg can't be built from a path with spaces in it (${snx_ffmpeg_src}). Use a build folder "
                        "whose path has none. FFmpeg is required by the NX runtime.")
endif()
set(snx_ffmpeg_patch "${CMAKE_SOURCE_DIR}/cmake/ffmpeg-vp9-nvdec.patch")
file(SHA256 "${snx_ffmpeg_patch}" snx_ffmpeg_patch_sha256)
include("${CMAKE_SOURCE_DIR}/cmake/patched_copy.cmake")
snx_patched_copy(SOURCE "${FFmpeg_SOURCE_DIR}" DEST "${snx_ffmpeg_src}" PATCHES "${snx_ffmpeg_patch}"
                 FILES libavcodec/vp9.c 69bcc226b8d240fb21ea5cfa071c101ed451409bf80368ec26d5664c9fc2ab6c)

find_program(SNX_FFMPEG_SH NAMES sh bash)
find_program(SNX_FFMPEG_MAKE NAMES gmake make)
if(NOT SNX_FFMPEG_SH OR NOT SNX_FFMPEG_MAKE)
    message(FATAL_ERROR "Building FFmpeg needs a POSIX shell (sh) and GNU make (gmake on the BSDs) on the PATH. On "
                        "Windows, scripts\\bootstrap.ps1 installs them with w64devkit. FFmpeg is required by the NX runtime.")
endif()



list(JOIN CMAKE_C_COMPILER_LAUNCHER " " snx_ffmpeg_launcher)
set(snx_ffmpeg_make_cc "")
if(snx_ffmpeg_launcher)
    set(snx_ffmpeg_make_cc "CC=${snx_ffmpeg_launcher} ${CMAKE_C_COMPILER}")
endif()
set(snx_ffmpeg_args
    --enable-static
    --disable-shared
    --enable-pic
    --disable-autodetect
    --disable-everything
    --disable-programs
    --disable-doc
    --disable-network
    --disable-avdevice
    --disable-avformat
    --disable-avfilter
    --disable-swscale
    --disable-iconv
    --enable-decoder=h264,vp8,vp9,opus
    --cc=${CMAKE_C_COMPILER}
    --cxx=${CMAKE_CXX_COMPILER}
    --ar=${CMAKE_AR}
    --ranlib=${CMAKE_RANLIB}
    --nm=${CMAKE_NM}

    --extra-cflags=-w)
if(WIN32 OR CMAKE_CROSSCOMPILING)


    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(snx_ffmpeg_arch x86_64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" OR
           (APPLE AND NOT CMAKE_SYSTEM_PROCESSOR AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64"))

        set(snx_ffmpeg_arch aarch64)
    else()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" snx_ffmpeg_arch)
    endif()
    if(WIN32)
        set(snx_ffmpeg_os mingw32)
    elseif(APPLE)

        set(snx_ffmpeg_os darwin)
    else()

        string(TOLOWER "${CMAKE_SYSTEM_NAME}" snx_ffmpeg_os)
    endif()
    list(APPEND snx_ffmpeg_args --target-os=${snx_ffmpeg_os} --arch=${snx_ffmpeg_arch})
endif()
if(ANDROID)


    list(APPEND snx_ffmpeg_args --sysroot=${CMAKE_SYSROOT} --extra-cflags=--target=${CMAKE_C_COMPILER_TARGET}
         --extra-ldflags=--target=${CMAKE_C_COMPILER_TARGET} --enable-pic)
endif()
if(SNX_APPLE_FLAGS)



    list(JOIN SNX_APPLE_FLAGS " " snx_ffmpeg_apple_flags)
    list(APPEND snx_ffmpeg_args "--extra-cflags=${snx_ffmpeg_apple_flags}" "--extra-ldflags=${snx_ffmpeg_apple_flags}")
endif()
if(CMAKE_CROSSCOMPILING)

    include("${CMAKE_CURRENT_LIST_DIR}/host_cc.cmake")
    list(APPEND snx_ffmpeg_args --enable-cross-compile --host-cc=${SNX_HOST_CC})
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")

    option(SNX_FFMPEG_ASM "Build FFmpeg's x86-64 assembly (needs NASM)" ON)
    if(SNX_FFMPEG_ASM)
        find_program(SNX_NASM nasm)
        if(NOT SNX_NASM)
            message(FATAL_ERROR "NASM was not found: FFmpeg needs it for its x86-64 assembly. Install nasm, or configure "
                                "with -DSNX_FFMPEG_ASM=OFF to build FFmpeg without it (movies decode more slowly).")
        endif()
        list(APPEND snx_ffmpeg_args --x86asmexe=${SNX_NASM})
        execute_process(COMMAND "${SNX_NASM}" -v OUTPUT_VARIABLE snx_ffmpeg_nasm OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        list(APPEND snx_ffmpeg_args --disable-x86asm)
    endif()
endif()



snx_deps_cached(ffmpeg snx_ffmpeg_cache "source: ${snx_ffmpeg_sha256}" "patch: ${snx_ffmpeg_patch_sha256}"
                "configure: ${snx_ffmpeg_args}" "nasm: ${snx_ffmpeg_nasm}")
set(SNX_FFMPEG_PREBUILT "" CACHE PATH "Optional compatible FFmpeg 9.0.2 static install built with this recipe")
if(SNX_FFMPEG_PREBUILT)
    foreach(required include/libavutil/ffversion.h lib/libavcodec.a lib/libavutil.a lib/libswresample.a)
        if(NOT EXISTS "${SNX_FFMPEG_PREBUILT}/${required}")
            message(FATAL_ERROR "Incomplete FFmpeg installation: ${required}")
        endif()
    endforeach()
    file(READ "${SNX_FFMPEG_PREBUILT}/include/libavutil/ffversion.h" snx_prebuilt_version)
    if(NOT snx_prebuilt_version MATCHES "FFMPEG_VERSION \"9\\.0\\.2\"")
        message(FATAL_ERROR "The optional FFmpeg install must match pinned version 9.0.2")
    endif()
    set(snx_ffmpeg_cache_FOUND TRUE)
    set(snx_ffmpeg_cache_DIR "${SNX_FFMPEG_PREBUILT}")
endif()
if(snx_ffmpeg_cache_FOUND)
    set(snx_ffmpeg_prefix "${snx_ffmpeg_cache_DIR}")
else()

    cmake_host_system_information(RESULT snx_ffmpeg_jobs QUERY NUMBER_OF_LOGICAL_CORES)
    if("$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" MATCHES "^[1-9][0-9]*$")
        set(snx_ffmpeg_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
    endif()
    include(ExternalProject)
    ExternalProject_Add(ffmpeg_build
        SOURCE_DIR "${snx_ffmpeg_src}"


        BINARY_DIR "${CMAKE_BINARY_DIR}/ffmpeg-build-patched"
        INSTALL_DIR "${snx_ffmpeg_prefix}"
        CONFIGURE_COMMAND "${SNX_FFMPEG_SH}" "${snx_ffmpeg_src}/configure" --prefix=${snx_ffmpeg_prefix}
                          ${snx_ffmpeg_args}
        BUILD_COMMAND "${SNX_FFMPEG_MAKE}" -j${snx_ffmpeg_jobs} ${snx_ffmpeg_make_cc}
        INSTALL_COMMAND "${SNX_FFMPEG_MAKE}" install
        BUILD_BYPRODUCTS "${snx_ffmpeg_prefix}/lib/libavcodec.a" "${snx_ffmpeg_prefix}/lib/libswresample.a"
                         "${snx_ffmpeg_prefix}/lib/libavutil.a"

        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        LOG_OUTPUT_ON_FAILURE ON)
    snx_deps_publish(ffmpeg_build ffmpeg "${snx_ffmpeg_prefix}" "${snx_ffmpeg_cache_DIR}")
endif()


file(MAKE_DIRECTORY "${snx_ffmpeg_prefix}/include")
add_library(FFmpeg::avutil STATIC IMPORTED GLOBAL)


set_target_properties(FFmpeg::avutil PROPERTIES IMPORTED_LOCATION "${snx_ffmpeg_prefix}/lib/libavutil.a"
                                                INTERFACE_INCLUDE_DIRECTORIES "${snx_ffmpeg_prefix}/include"
                                                SYSTEM OFF)
if(TARGET ffmpeg_build)
    add_dependencies(FFmpeg::avutil ffmpeg_build)
endif()
if(WIN32)

    target_link_libraries(FFmpeg::avutil INTERFACE bcrypt)
else()
    target_link_libraries(FFmpeg::avutil INTERFACE Threads::Threads m)
endif()
add_library(FFmpeg::swresample STATIC IMPORTED GLOBAL)
set_target_properties(FFmpeg::swresample PROPERTIES IMPORTED_LOCATION "${snx_ffmpeg_prefix}/lib/libswresample.a"
                                                    SYSTEM OFF)
target_link_libraries(FFmpeg::swresample INTERFACE FFmpeg::avutil)
if(TARGET ffmpeg_build)
    add_dependencies(FFmpeg::swresample ffmpeg_build)
endif()
add_library(FFmpeg::avcodec STATIC IMPORTED GLOBAL)
set_target_properties(FFmpeg::avcodec PROPERTIES IMPORTED_LOCATION "${snx_ffmpeg_prefix}/lib/libavcodec.a" SYSTEM OFF)

target_link_libraries(FFmpeg::avcodec INTERFACE FFmpeg::swresample FFmpeg::avutil)
if(TARGET ffmpeg_build)
    add_dependencies(FFmpeg::avcodec ffmpeg_build)
endif()
