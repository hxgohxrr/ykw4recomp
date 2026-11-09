/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#include "nx/hle/services/services_int.h"
#include "hle/services/sm.swipc.inc"

static SvcObj* svc_generic(const char* name);  

static int valid_name(const nn_sm_ServiceName* name) {
    int i = 0;
    while (i < 8 && name->v[i]) i++;
    if (!i) return 0;
    for (; i < 8; i++)
        if (name->v[i]) return 0;
    return 1;
}
static int nn_sm_detail_IUserInterface_GetServiceHandle(SvcObj* self, const nn_sm_ServiceName* name, u32* session) {
    char n[9] = {0};
    SvcObj* s;
    if (!valid_name(name)) return RC(21, 6);  
    memcpy(n, name->v, 8);
    s = svc_open(n);
    if (!s) {
        WARN("sm: service '%s' does not exist in YKW4Recomp: using a stub\n", n);
        s = svc_generic(n);
    }
    LOG("sm: GetServiceHandle(%s)\n", n);
    *session = session_new(s);
    return 0;
}

static int nn_sm_detail_IUserInterface_RegisterService(SvcObj* self, const nn_sm_ServiceName* name, u8 is_light,
                                                       i32 max_sessions, u32* port) {
    return valid_name(name) ? RC(21, 8)   : RC(21, 6)  ;
}

static int nn_sm_detail_IUserInterface_UnregisterService(SvcObj* self, const nn_sm_ServiceName* name) {
    char n[9] = {0};
    if (!valid_name(name)) return RC(21, 6);  
    memcpy(n, name->v, 8);
    return svc_registered(n) ? RC(21, 8)   : RC(21, 7)  ;
}
SvcObj* new_sm(void) { return nn_sm_detail_IUserInterface_new(0); }


typedef struct {
    char name[16];
} GenData;
static int generic_disp(SvcObj* o, IpcCtx* c) {
    if (!g_opt_auto_stub_ipc) return ipc_unknown(o, c);
    if (ipc_warn_once(((GenData*)o->data)->name, c->cmd))
        WARN("  [not implemented] %s cmd %u -> ok\n", ((GenData*)o->data)->name, c->cmd);
    return 0;
}
static SvcObj* svc_generic(const char* n) {
    SvcObj* o = svcobj_new("generic", generic_disp, sizeof(GenData));
    strncpy(((GenData*)o->data)->name, n, 15);
    o->name = ((GenData*)o->data)->name;
    return o;
}
