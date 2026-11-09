/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_BACKEND_H
#define SNX_BACKEND_H
#include "common/cpu/aarch64/cpu.h"
#include "nx/runtime/ini.h"

void video_init(const char* title);
void video_poll(void);
extern const IniOpt g_backend_ini[];  

int backend_ini(const char* k, const char* v);

void backend_touch_set(int mode);
void frontend_frame(void);

void backend_window_size(int* w, int* h);

void backend_window_safe(int* x, int* y, int* w, int* h);

int backend_window_shown(void);
u32 backend_window_generation(void);

void win_fatal_box(const char* msg);


typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
void* backend_vk_loader(void);  
const char* const* backend_vk_extensions(u32* n);
int backend_vk_surface(VkInstance instance, VkSurfaceKHR* surface);

#endif
