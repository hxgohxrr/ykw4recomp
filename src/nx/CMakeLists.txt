# Modified for YKW4 Launcher. See docs/LOCAL-CHANGES.md (2026-10-24).
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors









add_library(snx_nx_flags INTERFACE)
target_link_libraries(snx_nx_flags INTERFACE YKW4Recomp-common YKW4Recomp-net YKW4Recomp-filesystem)



target_compile_options(snx_nx_flags INTERFACE -fno-strict-aliasing
                       $<$<COMPILE_LANGUAGE:C>:-Werror=missing-prototypes -Werror=missing-variable-declarations>)
target_compile_definitions(snx_nx_flags INTERFACE VK_NO_PROTOTYPES)
target_link_libraries(snx_nx_flags INTERFACE Vulkan::Headers)
target_include_directories(snx_nx_flags BEFORE INTERFACE "${volk_SOURCE_DIR}"
                                                "${VulkanMemoryAllocator_SOURCE_DIR}/include")

function(snx_component name)
    add_library(snx_${name} OBJECT ${ARGN})
    target_link_libraries(snx_${name} PUBLIC snx_nx_flags)
endfunction()

snx_component(core runtime/main.c runtime/arm.c runtime/loader.c runtime/rtapi.c runtime/clock.c runtime/crash.c runtime/options.c
              runtime/ini.c runtime/startup.c runtime/gdbstub.c runtime/present.c)
target_link_libraries(snx_core PRIVATE snx_driver snx_lz4)
if(APPLE)


    target_link_options(snx_core INTERFACE "LINKER:-U,_snx_module_static")
endif()
snx_component(kernel hle/kernel/kernel.c hle/kernel/thread.c hle/kernel/sync.c hle/svc/svc.c hle/svc/svc_mem.c hle/kernel/debug.c
              memory/mem.c hle/kernel/ipc.c)
snx_component(hle hle/services/services.c hle/services/audren.c hle/services/audren_dsp.c hle/services/pl.c hle/services/sm.c hle/services/am.c hle/services/set.c hle/services/time.c hle/services/acc.c hle/services/audio.c
              hle/services/misc.c hle/services/ldr.c hle/services/network/bsd.c hle/services/network/dns.c
              hle/services/network/nifm.c hle/services/network/ssl.c hle/filesystem/fs.c hle/services/hid.c hle/services/hid_motion.c hle/services/hid_touch.c hle/services/vi.c hle/services/nvdrv.c)

target_sources(snx_hle PRIVATE "${CMAKE_BINARY_DIR}/generated/shared_fonts.c")


include("${PROJECT_SOURCE_DIR}/cmake/swipc.cmake")
snx_swipc(snx_hle ROOT "${CMAKE_CURRENT_SOURCE_DIR}" FILES
          hle/services/types.swipc hle/services/sm.swipc hle/services/set.swipc hle/services/acc.swipc
          hle/services/audio.swipc hle/services/ldr.swipc hle/services/pl.swipc hle/services/time.swipc
          hle/services/misc.swipc hle/services/am.swipc hle/services/hid.swipc
          hle/services/vi.swipc hle/services/nvdrv.swipc hle/services/network/bsd.swipc
          hle/services/network/dns.swipc hle/services/network/nifm.swipc hle/services/network/ssl.swipc
          hle/filesystem/fs.swipc)
snx_component(video gpu/video/host1x.c gpu/video/nvdec.c gpu/video/vp9_rebuild.cpp gpu/video/vic.c)
target_link_libraries(snx_video PRIVATE FFmpeg::avcodec)

target_link_libraries(snx_hle PRIVATE FFmpeg::avcodec OpenSSL::SSL)
snx_component(gpu gpu/gpu.c gpu/maxwell/engines.c gpu/maxwell/mme.c gpu/maxwell/compute.c gpu/maxwell/fermi2d.c gpu/maxwell/dma.c gpu/maxwell/write_pages.c gpu/maxwell/maxwell3d.c gpu/renderer.c)

include("${PROJECT_SOURCE_DIR}/cmake/controls_atlas.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/gui_themes.cmake")
set(platform runtime/platform/plat.c runtime/platform/unattended.c runtime/platform/backend/sdl3.c
             runtime/platform/backend/gamepad.c runtime/platform/input_prompts.c
             runtime/platform/overlay.c runtime/platform/touch.c runtime/platform/ui.c runtime/platform/vpad.c
             runtime/platform/fatal_screen.c "${CMAKE_BINARY_DIR}/generated/controls_png.c")
if(WIN32)
    list(APPEND platform runtime/platform/plat_win32.c runtime/platform/profiler_win32.c runtime/discord_presence.c)
else()
    list(APPEND platform runtime/platform/plat_posix.c)
endif()
snx_component(platform ${platform})
if(WIN32)
    target_sources(snx_platform PRIVATE runtime/launcher_binding.c runtime/launcher_machine.c)
    target_link_libraries(snx_platform PUBLIC bcrypt crypt32 advapi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    target_sources(snx_platform PRIVATE runtime/launcher_binding_linux.c runtime/launcher_machine_linux.c)
    target_compile_definitions(snx_platform PRIVATE YKW4_DESKTOP_LAUNCHER=1)
    target_link_libraries(snx_platform PUBLIC OpenSSL::Crypto ${CMAKE_DL_LIBS})
endif()

snx_component(shader gpu/shader/shader.c gpu/shader/decode.c gpu/shader/corpus.c
                     gpu/shader/spirv_translate.c gpu/shader/spirv_compute.c gpu/shader/spirv_emit.c)
snx_component(vk gpu/vk/renderer.c gpu/vk/device.c gpu/vk/device_fault.c gpu/vk/cmd.c gpu/vk/format.c gpu/vk/surface.c gpu/vk/texture.c
                 gpu/vk/buffer.c gpu/vk/writeback.c gpu/vk/pipeline.c gpu/vk/render.c gpu/vk/release.c gpu/vk/compute.c gpu/vk/occlusion.c
                 gpu/vk/pending_pages.c gpu/vk/astc.c gpu/vk/ui.c gpu/texture/etc2.c gpu/texture/depth.c gpu/vk/vic.c gpu/vk/vma.cpp "${volk_SOURCE_DIR}/volk.c")
set(components core kernel hle video gpu platform shader vk)

set_source_files_properties("${volk_SOURCE_DIR}/volk.c" gpu/vk/vma.cpp PROPERTIES COMPILE_OPTIONS -w)

if(ANDROID)


    add_library(YKW4Recomp SHARED runtime/entry.c)

    target_link_options(YKW4Recomp PRIVATE -Wl,-Bsymbolic)
    add_executable(YKW4Recomp-run runtime/platform/android_run.c)
    set_target_properties(YKW4Recomp-run PROPERTIES OUTPUT_NAME libYKW4Recomp-run.so RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    target_link_libraries(YKW4Recomp-run PRIVATE ${CMAKE_DL_LIBS})
    install(TARGETS YKW4Recomp YKW4Recomp-run LIBRARY DESTINATION lib/${ANDROID_ABI} RUNTIME DESTINATION lib/${ANDROID_ABI})
else()
    add_executable(YKW4Recomp runtime/entry.c)
    if(NOT IOS)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
        configure_file("${CMAKE_SOURCE_DIR}/scripts/mobile_builder.py"
                       "${CMAKE_BINARY_DIR}/tools/mobile_builder.py" COPYONLY)
    endif()

    if(TARGET YKW4Recomp-gui)
        target_link_libraries(YKW4Recomp PRIVATE YKW4Recomp-gui)
        target_compile_definitions(YKW4Recomp PRIVATE SNX_WITH_GUI)
    endif()
endif()
if(IOS)

    set_target_properties(YKW4Recomp PROPERTIES MACOSX_BUNDLE ON
                          MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/dist/apple/Info-iOS.plist"
                          MACOSX_BUNDLE_GUI_IDENTIFIER org.YKW4Recomp
                          MACOSX_BUNDLE_BUNDLE_NAME YKW4Recomp
                          MACOSX_BUNDLE_SHORT_VERSION_STRING 0.1
                          MACOSX_BUNDLE_BUNDLE_VERSION 1)
endif()
list(TRANSFORM components PREPEND snx_)
add_library(YKW4Recomp-nx STATIC)
set_target_properties(YKW4Recomp-nx PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(YKW4Recomp-nx PUBLIC snx_nx_flags ${components} snx_driver)
target_link_libraries(YKW4Recomp PRIVATE YKW4Recomp-nx)
if(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
    target_sources(YKW4Recomp PRIVATE runtime/launcher_cli.c)
    target_compile_definitions(YKW4Recomp PRIVATE YKW4_DESKTOP_LAUNCHER=1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    target_sources(YKW4Recomp PRIVATE runtime/launcher_linux.c)
    foreach(helper desktop_launcher.py launcher_core.py discord_linux.py)
        configure_file("${PROJECT_SOURCE_DIR}/scripts/${helper}" "${CMAKE_BINARY_DIR}/tools/${helper}" COPYONLY)
    endforeach()
    configure_file("${PROJECT_SOURCE_DIR}/scripts/switchprep/switchprep.py" "${CMAKE_BINARY_DIR}/tools/switchprep.py" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/dist/icons/launcher-logo.png" "${CMAKE_BINARY_DIR}/tools/launcher-logo.png" COPYONLY)
    foreach(language es en ja de)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/${language}.json" "${CMAKE_BINARY_DIR}/tools/langs/${language}.json" COPYONLY)
    endforeach()
endif()


set_target_properties(YKW4Recomp PROPERTIES LINKER_LANGUAGE CXX RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(WIN32)



      target_link_libraries(YKW4Recomp PRIVATE user32 comctl32 winhttp bcrypt crypt32 advapi32 oleaut32 uuid)
      if(SNX_YKW4_DISTRIBUTABLE)
        target_compile_definitions(YKW4Recomp PRIVATE YKW4_DISTRIBUTABLE=1)
      endif()
      add_library(ykw4_discord_json STATIC runtime/vendor/cjson/cJSON.c)
      target_link_libraries(snx_platform PRIVATE ykw4_discord_json)
      add_library(ykw4_launcher_support STATIC
          runtime/vendor/miniz/miniz.c runtime/vendor/miniz/miniz_tinfl.c
          runtime/vendor/miniz/miniz_tdef.c runtime/vendor/miniz/miniz_zip.c)
      target_include_directories(ykw4_launcher_support PUBLIC runtime/vendor/cjson runtime/vendor/miniz)
      target_link_libraries(ykw4_launcher_support PUBLIC ykw4_discord_json)
      target_compile_definitions(ykw4_launcher_support PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
      target_link_libraries(YKW4Recomp PRIVATE ykw4_launcher_support msimg32)
      target_sources(YKW4Recomp PRIVATE runtime/translation_win32.c runtime/launcher_security.c runtime/launcher_launch.c runtime/launcher_icon.c runtime/launcher_mods.c runtime/launcher_zip.c runtime/launcher_lang.c runtime/launcher_catalog.c runtime/launcher_setup.c runtime/launcher_update.c runtime/launcher_prepare.c runtime/launcher_win32.rc)
      set(language_resource "${CMAKE_BINARY_DIR}/generated/launcher_languages.rc")
      file(GENERATE OUTPUT "${language_resource}" CONTENT
          "400 RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/es.json\"\n401 RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/en.json\"\n402 RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/ja.json\"\n403 RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/de.json\"\n")
      set_source_files_properties("${language_resource}" PROPERTIES OBJECT_DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/es.json;${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/en.json;${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/ja.json;${CMAKE_CURRENT_SOURCE_DIR}/runtime/langs/de.json")
      target_sources(YKW4Recomp PRIVATE "${language_resource}")
      if(MINGW)
        set_source_files_properties(runtime/launcher_win32.rc PROPERTIES COMPILE_FLAGS "--codepage=65001")
      endif()

    enable_language(RC)
    set(SNX_SWITCHPREP_EXECUTABLE "" CACHE FILEPATH "Optional package preparation executable embedded in the Windows launcher")
    if(SNX_SWITCHPREP_EXECUTABLE)
        if(NOT EXISTS "${SNX_SWITCHPREP_EXECUTABLE}")
            message(FATAL_ERROR "Package preparation executable does not exist")
        endif()
        file(TO_CMAKE_PATH "${SNX_SWITCHPREP_EXECUTABLE}" package_helper_path)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        set(package_helper_resource "${CMAKE_BINARY_DIR}/generated/package_helper.rc")
        file(WRITE "${package_helper_resource}" "5001 RCDATA \"${package_helper_path}\"\n")
        set_source_files_properties("${package_helper_resource}" PROPERTIES OBJECT_DEPENDS "${SNX_SWITCHPREP_EXECUTABLE}")
        target_sources(YKW4Recomp PRIVATE "${package_helper_resource}")
    endif()
    target_sources(YKW4Recomp PRIVATE runtime/platform/runtime.rc)
    set_source_files_properties(runtime/platform/runtime.rc PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/runtime.manifest;${PROJECT_SOURCE_DIR}/dist/icons/launcher-normal.ico;${PROJECT_SOURCE_DIR}/dist/icons/launcher-dlc.ico")
    target_link_options(YKW4Recomp PRIVATE -static)
    target_link_options(YKW4Recomp PRIVATE -mwindows)
else()


    target_link_libraries(YKW4Recomp PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()


include("${PROJECT_SOURCE_DIR}/cmake/window_icon.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/build_info.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/pad_sdl.cmake")
target_link_libraries(snx_platform PRIVATE SDL3::SDL3 snx_window_icon snx_build_info snx_pad_sdl)

target_include_directories(snx_platform PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(YKW4Recomp PRIVATE SDL3::SDL3)

if(BUILD_TESTING)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
        add_library(ykw4-binding-linux-fixture SHARED "${PROJECT_SOURCE_DIR}/tests/binding_fixture.c")
        add_executable(ykw4-test-binding-linux "${PROJECT_SOURCE_DIR}/tests/launcher_binding_linux.c" runtime/launcher_binding_linux.c)
        snx_test_target(ykw4-test-binding-linux 11)
        target_include_directories(ykw4-test-binding-linux PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
        target_link_libraries(ykw4-test-binding-linux PRIVATE OpenSSL::Crypto ${CMAKE_DL_LIBS})
        add_executable(ykw4-test-machine-linux "${PROJECT_SOURCE_DIR}/tests/launcher_machine.c" runtime/launcher_machine_linux.c)
        snx_test_target(ykw4-test-machine-linux 11)
        target_include_directories(ykw4-test-machine-linux PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
        target_link_libraries(ykw4-test-machine-linux PRIVATE OpenSSL::Crypto)
        add_test(NAME launcher_machine_binding COMMAND ykw4-test-binding-linux "$<TARGET_FILE:ykw4-binding-linux-fixture>")
        add_test(NAME launcher_machine_identity COMMAND ykw4-test-machine-linux)
        if(Python3_Interpreter_FOUND)
            add_test(NAME launcher_runtime_binding COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/launcher_runtime_binding_test.py" --runtime "$<TARGET_FILE:YKW4Recomp>" --fixture "$<TARGET_FILE:ykw4-binding-linux-fixture>")
        endif()
    endif()
    if(WIN32)
        add_library(ykw4-binding-fixture SHARED "${PROJECT_SOURCE_DIR}/tests/binding_fixture.c")
        add_executable(ykw4-test-binding "${PROJECT_SOURCE_DIR}/tests/launcher_binding.c" runtime/launcher_binding.c)
        snx_test_target(ykw4-test-binding 11)
        target_include_directories(ykw4-test-binding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
        target_link_libraries(ykw4-test-binding PRIVATE bcrypt crypt32 advapi32)
        add_executable(ykw4-test-machine "${PROJECT_SOURCE_DIR}/tests/launcher_machine.c" runtime/launcher_machine.c)
        snx_test_target(ykw4-test-machine 11)
        target_include_directories(ykw4-test-machine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
        target_link_libraries(ykw4-test-machine PRIVATE bcrypt)
        if(MINGW)
            target_link_options(ykw4-binding-fixture PRIVATE -static)
            target_link_options(ykw4-test-binding PRIVATE -static)
            target_link_options(ykw4-test-machine PRIVATE -static)
        endif()
        add_test(NAME launcher_machine_binding COMMAND ykw4-test-binding "$<TARGET_FILE:ykw4-binding-fixture>")
        add_test(NAME launcher_machine_identity COMMAND ykw4-test-machine)
        if(Python3_Interpreter_FOUND)
            add_test(NAME launcher_runtime_binding COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/launcher_runtime_binding_test.py" --runtime "$<TARGET_FILE:YKW4Recomp>" --fixture "$<TARGET_FILE:ykw4-binding-fixture>")
        endif()
    endif()
    add_executable(ykw4-test-device-fault "${PROJECT_SOURCE_DIR}/tests/device_fault.c" gpu/vk/device_fault.c)
    snx_test_target(ykw4-test-device-fault 11)
    target_link_libraries(ykw4-test-device-fault PRIVATE snx_nx_flags)
    if(MINGW)
        target_link_options(ykw4-test-device-fault PRIVATE -static)
    endif()
    add_test(NAME device_fault_report COMMAND ykw4-test-device-fault)
    add_executable(ykw4-test-input-device "${PROJECT_SOURCE_DIR}/tests/input_device.c")
    snx_test_target(ykw4-test-input-device 11)
    target_include_directories(ykw4-test-input-device PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME input_device COMMAND ykw4-test-input-device)
    add_executable(ykw4-test-input-prompts "${PROJECT_SOURCE_DIR}/tests/input_prompts.c" runtime/platform/input_prompts.c)
    snx_test_target(ykw4-test-input-prompts 11)
    target_include_directories(ykw4-test-input-prompts PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(ykw4-test-input-prompts PRIVATE YKW4Recomp-common)
    add_test(NAME input_prompts COMMAND ykw4-test-input-prompts)
    add_executable(snx-test-shader-direct "${PROJECT_SOURCE_DIR}/tests/shader_direct.c")
    snx_test_target(snx-test-shader-direct 11)
    target_link_libraries(snx-test-shader-direct PRIVATE snx_shader)
    if(NOT WIN32)
        target_link_libraries(snx-test-shader-direct PRIVATE Threads::Threads m)
    endif()
    add_test(NAME shader_direct_structure COMMAND $<TARGET_FILE:snx-test-shader-direct> --check-only)
    add_executable(snx-test-shader-direct-regressions "${PROJECT_SOURCE_DIR}/tests/shader_direct_regressions.c")
    snx_test_target(snx-test-shader-direct-regressions 11)
    target_link_libraries(snx-test-shader-direct-regressions PRIVATE snx_shader)
    if(NOT WIN32)
        target_link_libraries(snx-test-shader-direct-regressions PRIVATE Threads::Threads m)
    endif()
    add_test(NAME shader_direct_regressions COMMAND $<TARGET_FILE:snx-test-shader-direct-regressions>)
    add_test(NAME runtime_settings_list COMMAND "${CMAKE_COMMAND}" -DRUNTIME=$<TARGET_FILE:YKW4Recomp>
             -P "${PROJECT_SOURCE_DIR}/tests/settings_list.cmake")
    add_test(NAME runtime_unknown_option COMMAND "${CMAKE_COMMAND}" -DRUNTIME=$<TARGET_FILE:YKW4Recomp>
             -P "${PROJECT_SOURCE_DIR}/tests/unknown_option.cmake")
    if(NOT WIN32)  
        add_test(NAME runtime_game_copy COMMAND "${CMAKE_COMMAND}" -DRUNTIME=$<TARGET_FILE:YKW4Recomp>
                 -P "${PROJECT_SOURCE_DIR}/tests/game_copy.cmake")
    endif()
    find_program(SNX_SPIRV_VAL spirv-val)
    find_program(SNX_SPIRV_DIS spirv-dis)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(SNX_SPIRV_VAL AND SNX_SPIRV_DIS AND Python3_Interpreter_FOUND)
        add_test(NAME shader_direct COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/shader_direct_test.py"
            --generator $<TARGET_FILE:snx-test-shader-direct> --validator "${SNX_SPIRV_VAL}"
            --disassembler "${SNX_SPIRV_DIS}"
            --regressions $<TARGET_FILE:snx-test-shader-direct-regressions>)
    else()
        message(STATUS "Skipping shader_direct: Python3, spirv-val, and spirv-dis are required")
    endif()
endif()


if(BUILD_TESTING AND SNX_TEST_VULKAN)
    add_executable(snx-test-vulkan "${PROJECT_SOURCE_DIR}/tests/vulkan_headless.c"
        "${PROJECT_SOURCE_DIR}/tests/spirv_emitter_fixture.c")
    target_link_libraries(snx-test-vulkan PRIVATE snx_nx_flags ${components} snx_driver Threads::Threads ${CMAKE_DL_LIBS})
    set_target_properties(snx-test-vulkan PROPERTIES LINKER_LANGUAGE CXX)
    if(WIN32)
        target_sources(snx-test-vulkan PRIVATE runtime/launcher_icon.c)
        if(MINGW)
            target_link_options(snx-test-vulkan PRIVATE -static)
        endif()
        target_link_libraries(snx-test-vulkan PRIVATE user32 gdi32 msimg32 bcrypt crypt32 advapi32)
    else()
        target_link_libraries(snx-test-vulkan PRIVATE m)
    endif()
    target_link_libraries(snx-test-vulkan PRIVATE SDL3::SDL3)
    add_test(NAME vulkan_headless COMMAND snx-test-vulkan -H)
    set_tests_properties(vulkan_headless PROPERTIES TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "vulkan validation ERROR;failed: VK_ERROR")
endif()
if(BUILD_TESTING)
    snx_test_sanitize(snx_shader)  
    snx_sanitizer_path()           
endif()


if(SNX_DEPLOY AND SNX_TEST_SANITIZERS AND BUILD_TESTING)
    message(STATUS "SNX_TEST_SANITIZERS: the runtime is not deployed to the games")
elseif(SNX_DEPLOY)
    add_custom_command(TARGET YKW4Recomp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-DRUNTIME=$<TARGET_FILE:YKW4Recomp>" "-DGAMES=${SNX_GAMES_DIR}" "-DWINDOWS=${WIN32}"
                "-DMACOS=${APPLE}"
                -P "${CMAKE_SOURCE_DIR}/cmake/deploy.cmake"
        VERBATIM)
endif()


if(ANDROID OR IOS)  
    set(snx_models "")
else()
    set(snx_models erista mariko aula)
endif()
foreach(model ${snx_models})
    if(WIN32)
        add_custom_command(TARGET YKW4Recomp POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:YKW4Recomp>"
                    "${CMAKE_BINARY_DIR}/YKW4Recomp-${model}.exe" VERBATIM)
        install(PROGRAMS "${CMAKE_BINARY_DIR}/YKW4Recomp-${model}.exe" DESTINATION bin)
    else()
        add_custom_command(TARGET YKW4Recomp POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink "$<TARGET_FILE_NAME:YKW4Recomp>"
                    "${CMAKE_BINARY_DIR}/YKW4Recomp-${model}" VERBATIM)
        install(PROGRAMS "${CMAKE_BINARY_DIR}/YKW4Recomp-${model}" DESTINATION bin)
    endif()
endforeach()
