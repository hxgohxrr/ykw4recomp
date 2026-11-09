# Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24).
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors








set(snx_openssl_src "${OpenSSL_SOURCE_DIR}")
set(snx_openssl_prefix "${CMAKE_BINARY_DIR}/openssl")
find_package(Perl)
find_program(SNX_OPENSSL_MAKE NAMES gmake make)
if(NOT PERL_FOUND OR NOT SNX_OPENSSL_MAKE)
    message(FATAL_ERROR "Building OpenSSL needs Perl and GNU make (gmake on the BSDs) on the PATH. On Windows, Perl "
                        "comes with Git for Windows or Strawberry Perl. OpenSSL is required.")
endif()





set(snx_openssl_target "")
set(snx_openssl_env "")
if(ANDROID)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(snx_openssl_target android-arm64)
        set(snx_openssl_android_cc aarch64-linux-android${ANDROID_PLATFORM_LEVEL}-clang)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(snx_openssl_target android-x86_64)
        set(snx_openssl_android_cc x86_64-linux-android${ANDROID_PLATFORM_LEVEL}-clang)
    else()
        message(FATAL_ERROR "OpenSSL's Android build is only set up for arm64-v8a and x86_64 (cmake/openssl.cmake)")
    endif()
    set(snx_openssl_env "${CMAKE_COMMAND}" -E env "ANDROID_NDK_ROOT=${CMAKE_ANDROID_NDK}"
                        "PATH=${ANDROID_TOOLCHAIN_ROOT}/bin:$ENV{PATH}")
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(snx_openssl_target mingw-arm64)
    else()
        set(snx_openssl_target mingw64)
    endif()
elseif(IOS)

    set(snx_openssl_target ios64-xcrun)
elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Cross-building OpenSSL is only set up for Windows, Android and iOS (cmake/openssl.cmake)")
endif()
set(snx_openssl_args
    ${snx_openssl_target}
    --libdir=lib
    no-shared
    no-module
    no-apps
    no-docs
    no-tests
    no-autoload-config)



set(snx_openssl_cc "${CMAKE_C_COMPILER}")
if(SNX_APPLE_FLAGS)
    list(JOIN SNX_APPLE_FLAGS " " snx_openssl_apple_flags)
    string(APPEND snx_openssl_cc " ${snx_openssl_apple_flags}")
endif()
if(ANDROID)
    list(APPEND snx_openssl_args -D__ANDROID_API__=${ANDROID_PLATFORM_LEVEL})
else()
    list(APPEND snx_openssl_args "CC=${snx_openssl_cc}" AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB})
endif()
if(CMAKE_RC_COMPILER)
    list(APPEND snx_openssl_args RC=${CMAKE_RC_COMPILER})
endif()


if(IOS)
    list(APPEND snx_openssl_args no-async -miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()
if(NOT WIN32)

    list(APPEND snx_openssl_args -fPIC)
endif()



set(snx_openssl_make_cc "")
set(snx_openssl_make_perl "")
if(WIN32)

    set(snx_openssl_make_perl "PERL=${PERL_EXECUTABLE}")
endif()
list(JOIN CMAKE_C_COMPILER_LAUNCHER " " snx_openssl_launcher)
if(snx_openssl_launcher AND ANDROID)
    set(snx_openssl_make_cc "CC=${snx_openssl_launcher} ${snx_openssl_android_cc}")
elseif(snx_openssl_launcher)
    set(snx_openssl_make_cc "CC=${snx_openssl_launcher} ${snx_openssl_cc}")
endif()




snx_deps_cached(openssl snx_openssl_cache "source: ${snx_openssl_sha256}" "configure: ${snx_openssl_args}"
                "android: ${CMAKE_ANDROID_NDK} ${snx_openssl_android_cc}" "perl: ${PERL_VERSION_STRING}")
set(SNX_OPENSSL_PREBUILT "" CACHE PATH "Optional compatible OpenSSL 4.0.3 static install")
if(SNX_OPENSSL_PREBUILT)
    foreach(required include/openssl/opensslv.h lib/libcrypto.a lib/libssl.a)
        if(NOT EXISTS "${SNX_OPENSSL_PREBUILT}/${required}")
            message(FATAL_ERROR "Incomplete OpenSSL installation: ${required}")
        endif()
    endforeach()
    file(READ "${SNX_OPENSSL_PREBUILT}/include/openssl/opensslv.h" snx_prebuilt_version)
    if(NOT snx_prebuilt_version MATCHES "OPENSSL_VERSION_TEXT \"OpenSSL 4\\.0\\.3 ")
        message(FATAL_ERROR "The optional OpenSSL install must match pinned version 4.0.3")
    endif()
    set(snx_openssl_cache_FOUND TRUE)
    set(snx_openssl_cache_DIR "${SNX_OPENSSL_PREBUILT}")
endif()
if(snx_openssl_cache_FOUND)
    set(snx_openssl_prefix "${snx_openssl_cache_DIR}")
else()

    cmake_host_system_information(RESULT snx_openssl_jobs QUERY NUMBER_OF_LOGICAL_CORES)
    if("$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" MATCHES "^[1-9][0-9]*$")
        set(snx_openssl_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
    endif()
    include(ExternalProject)
    ExternalProject_Add(openssl_build
        SOURCE_DIR "${snx_openssl_src}"
        BINARY_DIR "${CMAKE_BINARY_DIR}/openssl-build"
        INSTALL_DIR "${snx_openssl_prefix}"
        CONFIGURE_COMMAND ${snx_openssl_env} "${PERL_EXECUTABLE}" "${snx_openssl_src}/Configure"
                          --prefix=${snx_openssl_prefix} ${snx_openssl_args}
        BUILD_COMMAND ${snx_openssl_env} "${SNX_OPENSSL_MAKE}" -j${snx_openssl_jobs} build_libs ${snx_openssl_make_cc} ${snx_openssl_make_perl}

        INSTALL_COMMAND ${snx_openssl_env} "${SNX_OPENSSL_MAKE}" install_dev ${snx_openssl_make_perl}
        BUILD_BYPRODUCTS "${snx_openssl_prefix}/lib/libcrypto.a" "${snx_openssl_prefix}/lib/libssl.a"
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        LOG_OUTPUT_ON_FAILURE ON)
    if(WIN32)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        ExternalProject_Add_Step(openssl_build windows_archive_limit
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/openssl_archive_limit.py"
                    "${CMAKE_BINARY_DIR}/openssl-build/Makefile"
            DEPENDEES configure DEPENDERS build)
    endif()
    snx_deps_publish(openssl_build openssl "${snx_openssl_prefix}" "${snx_openssl_cache_DIR}")
endif()


file(MAKE_DIRECTORY "${snx_openssl_prefix}/include")
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)


set_target_properties(OpenSSL::Crypto PROPERTIES IMPORTED_LOCATION "${snx_openssl_prefix}/lib/libcrypto.a"
                                                 INTERFACE_INCLUDE_DIRECTORIES "${snx_openssl_prefix}/include"
                                                 SYSTEM OFF)
if(TARGET openssl_build)
    add_dependencies(OpenSSL::Crypto openssl_build)
endif()
if(WIN32)

    target_link_libraries(OpenSSL::Crypto INTERFACE bcrypt crypt32 ws2_32 advapi32 user32)
else()
    target_link_libraries(OpenSSL::Crypto INTERFACE Threads::Threads ${CMAKE_DL_LIBS})
endif()
add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::SSL PROPERTIES IMPORTED_LOCATION "${snx_openssl_prefix}/lib/libssl.a" SYSTEM OFF)
target_link_libraries(OpenSSL::SSL INTERFACE OpenSSL::Crypto)
if(TARGET openssl_build)
    add_dependencies(OpenSSL::SSL openssl_build)
endif()
