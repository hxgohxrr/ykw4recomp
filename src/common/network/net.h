/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_NET_H
#define SNX_NET_H
#include <stddef.h>
#include <stdint.h>

enum {
#define SNX_NET_ERRNO(name, value) SNX_E##name = value,
#include "common/network/abi/errno.inc"
#define SNX_NET_DOMAIN(name, value, host) SNX_AF_##name = value,
#include "common/network/abi/domain.inc"
#define SNX_NET_SOCKTYPE(name, value, host) SNX_SOCK_##name = value,
#include "common/network/abi/socktype.inc"
#define SNX_NET_LEVEL(name, value, host) SNX_SOL_##name = value,
#include "common/network/abi/level.inc"
#define SNX_NET_SOCKOPT(level, name, value, kind) SNX_##name = value,
#include "common/network/abi/sockopt.inc"
#define SNX_NET_MSG(name, value, host) SNX_MSG_##name = value,
#include "common/network/abi/msgflag.inc"
#define SNX_NET_POLL(name, value, host, wsapoll) SNX_POLL##name = value,
#include "common/network/abi/poll.inc"
#define SNX_NET_GAI(name, value, host, text) SNX_EAI_##name = value,
#include "common/network/abi/gai.inc"
#define SNX_NET_AIFLAG(name, value, host) SNX_AI_##name = value,
#include "common/network/abi/aiflag.inc"
#define SNX_NET_NIFLAG(name, value, host) SNX_NI_##name = value,
#include "common/network/abi/niflag.inc"
    SNX_SHUT_RD = 0,
    SNX_SHUT_WR = 1,
    SNX_SHUT_RDWR = 2,

    SNX_IPPROTO_IP = 0,
    SNX_IPPROTO_ICMP = 1,
    SNX_IPPROTO_TCP = 6,
    SNX_IPPROTO_UDP = 17,
    SNX_IPPROTO_ICMPV6 = 58,
    SNX_IPPROTO_RAW = 255,
};


typedef struct SnxSockAddr {
    uint16_t family;  
    uint16_t port;
    uint32_t flowinfo;  
    uint8_t addr[16];   
    uint32_t scope_id;  
} SnxSockAddr;


typedef struct SnxLinger {
    int32_t onoff, linger;
} SnxLinger;
typedef struct SnxTimeval {
    int64_t sec, usec;
} SnxTimeval;
typedef struct SnxIpMreq {
    uint8_t multiaddr[4], interface[4];
} SnxIpMreq;
typedef struct SnxIpv6Mreq {
    uint8_t multiaddr[16];
    uint32_t ifindex;
} SnxIpv6Mreq;

typedef struct SnxSocket SnxSocket;


int snx_net_init(void);

void snx_net_cancel(void);
void snx_net_restart(void);

void snx_net_set_log(void (*log)(const char* message));


int snx_socket_open(SnxSocket** out, int domain, int type, int protocol);
int snx_socket_pair(SnxSocket* out[2], int domain, int type, int protocol);

int snx_socket_eventfd(SnxSocket** out, uint64_t initial, int semaphore);
SnxSocket* snx_socket_ref(SnxSocket* s);
void snx_socket_unref(SnxSocket* s);  
int snx_socket_domain(const SnxSocket* s);
int snx_socket_type(const SnxSocket* s);
int snx_socket_protocol(const SnxSocket* s);
int snx_socket_emulated(const SnxSocket* s);  
int snx_socket_nonblocking(const SnxSocket* s);
int snx_socket_set_nonblocking(SnxSocket* s, int on);

int snx_socket_bind(SnxSocket* s, const SnxSockAddr* addr);
int snx_socket_connect(SnxSocket* s, const SnxSockAddr* addr);
int snx_socket_listen(SnxSocket* s, int backlog);

int snx_socket_accept(SnxSocket* s, SnxSocket** out, SnxSockAddr* peer, int nonblocking);
int snx_socket_shutdown(SnxSocket* s, int how);

int64_t snx_socket_send(SnxSocket* s, const void* data, size_t size, int flags, const SnxSockAddr* to);
int64_t snx_socket_recv(SnxSocket* s, void* data, size_t size, int flags, SnxSockAddr* from, int* out_flags);
int snx_socket_name(SnxSocket* s, SnxSockAddr* out);
int snx_socket_peer(SnxSocket* s, SnxSockAddr* out);

int snx_socket_setsockopt(SnxSocket* s, int level, int name, const void* value, size_t size);
int snx_socket_getsockopt(SnxSocket* s, int level, int name, void* value, size_t* size);
int snx_socket_readable_bytes(SnxSocket* s, int* bytes);  


typedef struct SnxPollEntry {
    SnxSocket* socket;
    intptr_t host_fd;
    uint16_t events, revents;
} SnxPollEntry;
int snx_net_poll(SnxPollEntry* entries, size_t count, int timeout_ms);


typedef struct SnxAddrInfo {
    int flags, family, socktype, protocol;
    SnxSockAddr addr;
    char* canonname;
    struct SnxAddrInfo* next;
} SnxAddrInfo;
int snx_net_getaddrinfo(const char* host, const char* service, const SnxAddrInfo* hints, SnxAddrInfo** out);
void snx_net_freeaddrinfo(SnxAddrInfo* list);
int snx_net_getnameinfo(const SnxSockAddr* addr, char* host, size_t host_size, char* service, size_t service_size,
                        int flags);
const char* snx_net_gai_strerror(int error);
int snx_net_parse_ipv4(const char* text, uint8_t out[4]);  


typedef struct SnxNetInterface {
    char name[64];
    uint8_t ip[4], mask[4], gateway[4];
    int wireless;
} SnxNetInterface;
int snx_net_interfaces(SnxNetInterface* out, int max);

int snx_net_primary_interface(SnxNetInterface* out);

int snx_net_dns_servers(uint8_t (*ipv4)[4], int max, char* nameservers_text, size_t text_size);

#endif
