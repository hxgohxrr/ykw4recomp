/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_LOG_H
#define SNX_LOG_H
#include <stdio.h>

extern int g_verbose;

void logf_(int lvl, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#define LOG(...) logf_(1, __VA_ARGS__)
#define LOGV(...)                                                                                                      \
    do {                                                                                                               \
        if (g_verbose >= 2) logf_(2, __VA_ARGS__);                                                                     \
    } while (0)
#define WARN(...) logf_(0, __VA_ARGS__)

#define LOG_LIMIT(n, ...)                                                                                              \
    do {                                                                                                               \
        static int log_limit_said_;                                                                                    \
        if (log_limit_said_ < (n) && __atomic_fetch_add(&log_limit_said_, 1, __ATOMIC_RELAXED) < (n))                  \
            LOG(__VA_ARGS__);                                                                                          \
    } while (0)

#define WARN_LIMIT(n, ...)                                                                                             \
    do {                                                                                                               \
        static int warn_limit_said_;                                                                                   \
        if (warn_limit_said_ < (n) && __atomic_fetch_add(&warn_limit_said_, 1, __ATOMIC_RELAXED) < (n))                \
            WARN(__VA_ARGS__);                                                                                         \
    } while (0)
void fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2), noreturn));

extern __thread FILE* tls_log_redirect;

void log_crash_mode(void);

#endif
