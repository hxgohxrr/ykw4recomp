/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_KERNEL_H
#define SNX_KERNEL_H
#include "common/cpu/aarch64/cpu.h"
#include "common/cpu/arm/arm.h"
#include "nx/runtime/platform/plat.h"

#define K_PROCESS_ID 0x51ull  
#define NCORES       4        

enum {
    KO_NONE,
    KO_EVENT,
    KO_THREAD,
    KO_SESSION,
    KO_PORT,
    KO_SHMEM,
    KO_TMEM,
    KO_PROCESS,
    KO_RESLIMIT,
    KO_CODEMEM,
    KO_SERVER,
    KO_WEVENT  
};
typedef struct KThread KThread;
typedef struct KObj {
    int type;
    int refs;
    void (*destroy)(struct KObj*);
    int (*is_signaled)(struct KObj*);
    KThread** waiters;
    int nwait, capwait;
    const char* name;
} KObj;

typedef struct KEvent {
    KObj o;
    int signaled;
} KEvent;

struct KThread {
    KObj o;
    CPU cpu;
    SnxArmCPU arm;  
    u64 id;
    int prio;            
    int base_prio;       
    int core;            
    int ideal_core;      
    u32 yields;          
    int on_core;         
    KThread* core_next;  
    u64 core_ticket;     
    u64 affinity;
    u64 tls;
    u64 entry, arg, stack_top;
    int state;  
    PCond cv;
    int woken, cancel;

    int suspended, in_svc, pause_at;

    int wait_kind;  
    u64 wait_addr, wait_key;
    u32 wait_tag;
    KThread* lock_owner;  
    int wait_result;
    KThread* next_waiter;
    char name[32];
    const char* wait_name;
    u64 wait_tid;  
    i64 wait_timeout;
    u64 sleep_lr, sleep_n, sleep_x[31];  
};

typedef struct KShmem {
    KObj o;
    u64 size;
    u8* host;
    u64 gaddr, gsize;  
    u32 perm;
} KShmem;
typedef struct KTmem {
    KObj o;
    u64 addr, size;
    u32 perm;
} KTmem;

void kernel_init(void);
u32 h_new(KObj* o);  
void kobj_ref(KObj* o);
void kobj_unref(KObj* o);
KObj* h_get_ref(u32 h, int t);  
KEvent* kevent_new(const char* name);
void kevent_signal(KEvent* e);
void kevent_clear(KEvent* e);
KThread* kthread_cur(void);
void core_enter(KThread* t);  
void core_leave(KThread* t);  
void core_poll(KThread* t);   
void kernel_run_main(u64 entry, u64 stack_size, int prio, int core);
void snx_svc(CPU* c, u32 imm);

void snx_svc32(SnxArmCPU* a, u32 imm);
extern PMutex g_klock;
extern u64 g_title_id;
extern u64 g_sysres_size;
extern int g_opt_mem_gb;

extern u64 g_core_mask, g_prio_mask;
KShmem* kshmem_new(u64 size, const char* name);
u8* kshmem_ptr(KShmem* s);
void kernel_load_symbols(void);  
void kernel_dump_regs(CPU* c);
void kernel_dump_threads(void);
void mem_report(char* out, int n);

#endif
