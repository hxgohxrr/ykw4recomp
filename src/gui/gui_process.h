/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_GUI_PROCESS_H
#define SNX_GUI_PROCESS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct GuiProcess GuiProcess;
typedef struct {
    uint64_t memory_bytes;  
    double cpu_percent;     
    bool available;
} GuiUsage;


typedef struct {
    double cpu, time;
    bool measured;
} GuiCpuBase;
static inline double gui_cpu_percent(GuiCpuBase* base, double cpu, double now) {
    double percent = 0;
    if (base->measured && now <= base->time && cpu >= base->cpu) return 0;
    if (base->measured && now > base->time && cpu >= base->cpu) percent = 100 * (cpu - base->cpu) / (now - base->time);
    base->cpu = cpu;
    base->time = now;
    base->measured = true;
    return percent;
}


GuiProcess* gui_process_create(void);
bool gui_process_start(GuiProcess* process, const char* const* argv, char* error, size_t size);

int gui_process_read(GuiProcess* process, char* buffer, size_t size);

bool gui_process_finished(GuiProcess* process, int* status);
GuiUsage gui_process_usage(GuiProcess* process);

void gui_process_terminate(GuiProcess* process);

void gui_process_destroy(GuiProcess* process);
#endif
