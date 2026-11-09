/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GUI_STEPS_H
#define SNX_GUI_STEPS_H
#include <stddef.h>
#include <stdint.h>


#define GUI_STEPS_MAX 24
#define GUI_NOTES_MAX 128
typedef enum { GUI_STEP_ACTIVE, GUI_STEP_DONE, GUI_STEP_FAILED } GuiStepState;
typedef struct {
    char label[12], detail[96];
    GuiStepState state;
    uint32_t value, total;  
    int megabytes;
    uint64_t shown_ms;
    uint32_t serial;
} GuiStep;

typedef struct {
    char text[112];
    uint64_t shown_ms;
    uint32_t step;  
} GuiNote;
typedef struct {
    GuiStep step[GUI_STEPS_MAX];  
    int count;
    GuiNote note[GUI_NOTES_MAX];  
    int notes;
    uint32_t serial;
    char line[256];
    size_t length;
    int gpu;
} GuiSteps;
void gui_steps_reset(GuiSteps* steps);
void gui_steps_feed(GuiSteps* steps, const char* data, size_t size, uint64_t now_ms);
void gui_steps_finish(GuiSteps* steps, int ok);

int gui_steps_percent(const GuiSteps* steps);

typedef enum { GUI_PROGRESS_AMOUNT, GUI_PROGRESS_PERCENT, GUI_PROGRESS_TIME } GuiProgress;

void gui_step_progress(const GuiStep* step, int how, uint64_t now_ms, char* out, size_t size);

const GuiStep* gui_steps_last(const GuiSteps* steps, const char* label);

int gui_step_notes(const GuiSteps* steps, const GuiStep* step, const GuiNote** out, int max);
#endif
