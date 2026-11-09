/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_HLE_NETWORK_H
#define SNX_HLE_NETWORK_H
#include "nx/hle/services/services_int.h"
#include "common/network/net.h"


void network_init(void);  
SvcObj* new_bsd_u(void);
SvcObj* new_bsd_s(void);
SvcObj* new_bsdcfg(void);

int network_online(void);
int network_interface(SnxNetInterface* out);  

int network_reachable(const SnxSockAddr* addr);

SnxSocket* bsd_client_socket(u64 pid, i32 fd);


SvcObj* new_sfdnsres(void);
SvcObj* new_nsd(void);


SvcObj* new_nifm(void);


SvcObj* new_ssl(void);

#endif
