/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_NET_INTERNAL_H
#define SNX_NET_INTERNAL_H
#include "common/network/net.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET SnxHostFd;
typedef SRWLOCK SnxLock;
typedef int snx_socklen;
#define SNX_BADFD INVALID_SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
typedef int SnxHostFd;
typedef pthread_mutex_t SnxLock;
typedef socklen_t snx_socklen;
#define SNX_BADFD (-1)
#endif


typedef struct SnxSocketOps {
    void (*destroy)(SnxSocket* s);
    int (*bind)(SnxSocket* s, const SnxSockAddr* addr);
    int (*connect)(SnxSocket* s, const SnxSockAddr* addr);
    int (*listen)(SnxSocket* s, int backlog);
    int (*accept)(SnxSocket* s, SnxSocket** out, SnxSockAddr* peer, int nonblocking);
    int (*shutdown)(SnxSocket* s, int how);
    int64_t (*send)(SnxSocket* s, const void* data, size_t size, int flags, const SnxSockAddr* to);
    int64_t (*recv)(SnxSocket* s, void* data, size_t size, int flags, SnxSockAddr* from, int* out_flags);
    int (*name)(SnxSocket* s, SnxSockAddr* out, int peer);
    int (*setsockopt)(SnxSocket* s, int level, int name, const void* value, size_t size);
    int (*getsockopt)(SnxSocket* s, int level, int name, void* value, size_t* size);
    int (*readable_bytes)(SnxSocket* s, int* bytes);

    uint16_t (*poll_ready)(SnxSocket* s, uint16_t events, short* host_events);
} SnxSocketOps;

struct SnxSocket {
    const SnxSocketOps* ops;
    SnxHostFd fd;  
    int domain, type, protocol;
    int nonblocking;
    int64_t rcvtimeo_us, sndtimeo_us;  
    int refs;
    void* impl;
};


SnxSocket* snx_net_socket_new(const SnxSocketOps* ops, SnxHostFd fd, int domain, int type, int protocol);
void snx_net_log(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
int snx_net_host_error(void);        
int snx_net_errno_from_host(int e);  
int snx_net_close_fd(SnxHostFd fd);
int snx_net_set_host_nonblocking(SnxHostFd fd);

int snx_net_wait(SnxHostFd fd, short host_events, int64_t timeout_us);
int snx_net_wait_until(SnxHostFd fd, short host_events, int64_t deadline);  
int64_t snx_net_deadline(int64_t timeout_us);                               

int snx_net_addr_to_host(const SnxSockAddr* in, struct sockaddr_storage* out, snx_socklen* size);
int snx_net_addr_from_host(const struct sockaddr* in, snx_socklen size, SnxSockAddr* out);
int snx_net_domain_to_host(int domain);
int snx_net_domain_from_host(int domain);
int snx_net_socktype_to_host(int type);  
int snx_net_socktype_from_host(int type);

int snx_net_host_setsockopt(SnxSocket* s, int level, int name, const void* value, size_t size);
int snx_net_host_getsockopt(SnxSocket* s, int level, int name, void* value, size_t* size);
int snx_net_sockopt_kept(int level, int name);
uint16_t snx_net_checksum(const void* data, size_t size, uint32_t sum);  
int64_t snx_net_now_us(void);                                            
void snx_net_sleep_us(int64_t us);


void snx_lock_init(SnxLock* l);
void snx_lock_destroy(SnxLock* l);
void snx_lock(SnxLock* l);
void snx_unlock(SnxLock* l);
int snx_net_thread(void (*fn)(void*), void* arg);
int snx_notifier_open(SnxHostFd out[2]);  
void snx_notifier_signal(SnxHostFd write_end);
void snx_notifier_drain(SnxHostFd read_end);


int snx_net_icmp_open(SnxSocket** out, int domain, int type, int protocol);  
int snx_net_event_open(SnxSocket** out, uint64_t initial, int semaphore);    

#endif
