/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_IPC_H
#define SNX_CTR_IPC_H
#include <stdint.h>
#define SNX_CTR_IPC_WORDS          64u
#define SNX_CTR_IPC_BAD_DESCRIPTOR 0xd9001830u
#define SNX_CTR_IPC_INVALID_STATE  0xe0a01bf5u
enum {
    SNX_CTR_SERVICE_NONE,
    SNX_CTR_SERVICE_SRV,
    SNX_CTR_SERVICE_FS,
    SNX_CTR_SERVICE_APT,
    SNX_CTR_SERVICE_PTM,
    SNX_CTR_SERVICE_HID,
    SNX_CTR_SERVICE_FILE,
    SNX_CTR_SERVICE_GSP,
    SNX_CTR_SERVICE_CFG,
    SNX_CTR_SERVICE_DSP
};
typedef enum { SNX_CTR_IPC_RETURN, SNX_CTR_IPC_UNSUPPORTED, SNX_CTR_IPC_BLOCKED, SNX_CTR_IPC_FAULT } SnxCtrIpcState;
struct SnxCtrProcess;

typedef struct {
    struct SnxCtrProcess* process;
    uint32_t object, caller_pid, transport_result;
    uint32_t in[SNX_CTR_IPC_WORDS], out[SNX_CTR_IPC_WORDS];

    uint32_t handle_in[2];
    SnxCtrIpcState state;
} SnxCtrIpc;
int snx_ctr_ipc_expect(SnxCtrIpc* c, uint32_t normal, uint32_t translated);
void snx_ctr_ipc_bad(SnxCtrIpc* c);
void snx_ctr_ipc_get_bytes(SnxCtrIpc* c, uint32_t first, uint8_t* bytes, uint32_t size);
void snx_ctr_ipc_put_bytes(SnxCtrIpc* c, uint32_t first, const uint8_t* bytes, uint32_t size);
void snx_ctr_ipc_error_reply(SnxCtrIpc* c, uint32_t result);

void snx_ctr_ipc_drop_handle(SnxCtrIpc* c, uint32_t handle);
void snx_ctr_ipc_output_handle(SnxCtrIpc* c, uint32_t word, uint32_t handle);
#endif
