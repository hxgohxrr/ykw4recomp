/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/cpu/aarch64/cpu.h"
#include "common/cpu/aarch64/cpu_helpers.c"

GuestFn snx_interp_entry;


#include "rtlib_interp.inc"

#include "libc.inc"


double round(double x);
float roundf(float x);
double round(double x) {
    union {
        double d;
        u64 u;
    } v;
    int e;
    v.d = x;
    e = (int)((v.u >> 52) & 0x7FF) - 1023;
    if (e >= 52) return x;  
    if (e < 0) {            
        v.u = (v.u & ((u64)1 << 63)) | (e == -1 ? (u64)0x3FF << 52 : 0);
        return v.d;
    }
    v.u += (u64)1 << (51 - e);           
    v.u &= ~(((u64)1 << (52 - e)) - 1);  
    return v.d;
}
float roundf(float x) {
    union {
        float f;
        u32 u;
    } v;
    int e;
    v.f = x;
    e = (int)((v.u >> 23) & 0xFF) - 127;
    if (e >= 23) return x;
    if (e < 0) {
        v.u = (v.u & ((u32)1 << 31)) | (e == -1 ? (u32)0x7F << 23 : 0);
        return v.f;
    }
    v.u += (u32)1 << (22 - e);
    v.u &= ~(((u32)1 << (23 - e)) - 1);
    return v.f;
}
#ifdef _WIN32

int DllMainCRTStartup(void* module, unsigned long reason, void* reserved);
int DllMainCRTStartup(void* module, unsigned long reason, void* reserved) {
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}
#endif
