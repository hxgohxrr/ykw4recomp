/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */
#define _GNU_SOURCE

#include "nx/runtime/platform/plat.h"
#include "nx/runtime/loader.h"
#include "nx/runtime/main.h"
#include "nx/runtime/platform/backend.h"
#include "nx/runtime/platform/log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

__thread void* tls_thread;
int g_verbose = 1;
static FILE* g_logf;
static int g_logf_failed;  

static char g_logpend[4096];
static size_t g_logpend_n;
static int g_logpend_cut;
static void logpend(const char* s) {
    size_t L = strlen(s);
    if (g_logpend_cut || L > sizeof g_logpend - g_logpend_n) {
        g_logpend_cut = 1;
        return;
    }
    memcpy(g_logpend + g_logpend_n, s, L);
    g_logpend_n += L;
}

static PMutex g_logm = PMUTEX_INIT;
static volatile int g_log_crash;  

__thread FILE* tls_log_redirect;  
void log_crash_mode(void) {
    g_log_crash = 1;
    tls_log_redirect = NULL;
}

static void log_raw(const char* buf) {
#if defined(_WIN32)
    if (g_logf) {
        fputs(buf, g_logf);
        fflush(g_logf);
    }
    fputs(buf, stderr);
#else
    size_t L = strlen(buf);
    if (g_logf && write(fileno(g_logf), buf, L) < 0) {}
    if (write(2, buf, L) < 0) {}
#endif
}
void logf_(int lvl, const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    if (n >= (int)sizeof buf) buf[sizeof buf - 2] = '\n';
    if (g_log_crash) {
        log_raw(buf);
        return;
    }
    {
        FILE* rf = tls_log_redirect;
        if (rf) {
            fputs(buf, rf);
            return;
        }
    }
    static u64 t0;
    u64 now = plat_ns();
    if (!t0) t0 = now;
    char tb[24];
    snprintf(tb, sizeof tb, "[%7.2f] ", (double)(now - t0) / 1e9);
    pm_lock(&g_logm);

    if (!g_logf && !g_logf_failed && g_mod) {
        char p[1200];
        snprintf(p, sizeof p, "%s%s.log", g_statedir, g_exename);
        g_logf = fopen(p, "w");
        if (!g_logf) {
            g_logf_failed = 1;
            fprintf(stderr, "Could not create the log %s, so it only goes to the console.\n", p);
        } else if (g_logpend_n)
            fwrite(g_logpend, 1, g_logpend_n, g_logf);
    }
    {
        static int bol = 1;
        if (g_logf) {
            if (bol) fputs(tb, g_logf);
            fputs(buf, g_logf);
            fflush(g_logf);
        } else if (!g_logf_failed) {
            if (bol) logpend(tb);
            logpend(buf);
        }
        size_t L = strlen(buf);
        bol = L && buf[L - 1] == '\n';
    }
    if (lvl <= 1 || g_verbose >= 2) { fputs(buf, stderr); }
    pm_unlock(&g_logm);
}
void fatal(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    tls_log_redirect = NULL;  
    logf_(0, "FATAL: %s\n", buf);
#if !defined(_WIN32)

    if (g_log_crash) _exit(1);
#endif

    win_fatal_box(buf);
    plat_exit_now(1);
}
