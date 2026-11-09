/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_CTR_DSP_SERVICE_H
#define SNX_CTR_DSP_SERVICE_H
#include "3ds/ipc/ctr_ipc.h"
#include <stdint.h>
struct SnxCtrProcess;

uint32_t snx_ctr_dsp_open(struct SnxCtrProcess* p);
void snx_ctr_dsp_dispatch(SnxCtrIpc* c);

void snx_ctr_dsp_frame(struct SnxCtrProcess* p, uint64_t now_ns);

void snx_ctr_dsp_component_file(struct SnxCtrProcess* p);
void snx_ctr_dsp_close(struct SnxCtrProcess* p);
#endif
