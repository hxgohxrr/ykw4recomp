#ifndef YKW4_INPUT_PROMPTS_H
#define YKW4_INPUT_PROMPTS_H
#include "nx/runtime/platform/input_device.h"
#include "nx/runtime/platform/ui.h"
#include "common/utilities/pad_map.h"

extern int g_input_prompts_mode;
const char* input_prompt_label(Ykw4InputFamily family, const SnxPadMap* map, int button);
void input_prompts_update(Ykw4InputFamily family, const SnxPadMap* map, int discord);
void input_prompts_draw(SnxUiList* list, const int safe[4]);
#endif
