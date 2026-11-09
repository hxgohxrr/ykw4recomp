# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors
add_library(YKW4Recomp-common STATIC
    utilities/common.c utilities/process.c utilities/font.c utilities/ini_lines.c utilities/pad_map.c
    utilities/pad_screen.c
    cpu/aarch64/a64.c cpu/recompiler/codegen.c cpu/recompiler/backend.c cpu/execution/execution.c)
target_compile_definitions(YKW4Recomp-common PRIVATE _CRT_SECURE_NO_WARNINGS
    SNX_HOST_COMPILER="${CMAKE_C_COMPILER}"
    SNX_CPU_INCLUDE="${CMAKE_CURRENT_SOURCE_DIR}/cpu/aarch64"
    SNX_SOURCE_INCLUDE="${PROJECT_SOURCE_DIR}/src")
target_compile_options(YKW4Recomp-common PRIVATE -fno-strict-aliasing -Wall -Wextra -Werror=missing-prototypes)
target_link_libraries(YKW4Recomp-common PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(YKW4Recomp-common PUBLIC advapi32)
else()
    target_link_libraries(YKW4Recomp-common PUBLIC m)
endif()



add_library(YKW4Recomp-filesystem STATIC filesystem/tree.c filesystem/host.c)
set_target_properties(YKW4Recomp-filesystem PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(YKW4Recomp-filesystem PRIVATE _GNU_SOURCE _FILE_OFFSET_BITS=64 _CRT_SECURE_NO_WARNINGS)
target_compile_options(YKW4Recomp-filesystem PRIVATE -Wall -Wextra -Werror=missing-prototypes)
target_link_libraries(YKW4Recomp-filesystem PUBLIC Threads::Threads)



add_library(YKW4Recomp-net STATIC network/net.c network/net_icmp.c network/net_event.c network/net_dns.c
    network/net_iface.c)
target_compile_options(YKW4Recomp-net PRIVATE -fno-strict-aliasing -Wall -Wextra -Werror=missing-prototypes)
target_link_libraries(YKW4Recomp-net PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(YKW4Recomp-net PUBLIC ws2_32 iphlpapi)
elseif(APPLE)  
    target_link_libraries(YKW4Recomp-net PUBLIC resolv)
endif()
