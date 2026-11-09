/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GNU_NETWORK_H
#define SNX_GNU_NETWORK_H
#include "gnuabi/process/process.h"
#include "common/network/net.h"
#include <stdbool.h>

enum {
#define LINUX_NET_SYSCALL(number, name) GNU_NR_##name = number,
#include "gnuabi/network/abi/linux_syscalls.inc"
};


bool gnu_net_syscall(GnuProcess* process, uint64_t number, const uint64_t arguments[6], int64_t* result);

int gnu_net_process_init(GnuProcess* process);


SnxSocket* gnu_net_pollable(const GnuFdInfo* info);
uint16_t gnu_net_poll_in(uint32_t guest);
uint32_t gnu_net_poll_out(uint16_t events);


int64_t gnu_net_ppoll(GnuProcess* process, const uint64_t arguments[6]);
int64_t gnu_net_pselect6(GnuProcess* process, const uint64_t arguments[6]);
int64_t gnu_net_epoll_create1(GnuProcess* process, uint64_t flags);
int64_t gnu_net_epoll_ctl(GnuProcess* process, const uint64_t arguments[6]);
int64_t gnu_net_epoll_pwait(GnuProcess* process, const uint64_t arguments[6], int timespec_timeout);
#endif
