/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GUI_MODEL_H
#define SNX_GUI_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common/utilities/font.h"  


enum {
    GUI_SWITCH,
    GUI_LINUX,
    GUI_3DS,
    GUI_KEYS,
    GUI_SETTINGS,  
    GUI_LOG,       
    GUI_MOBILE,
#ifdef __ANDROID__
    GUI_DATA,  
#endif
    GUI_EXIT,
    GUI_ITEM_COUNT
};
typedef enum { GUI_UP, GUI_DOWN, GUI_LEFT, GUI_RIGHT, GUI_SELECT, GUI_CANCEL, GUI_PAGE_UP, GUI_PAGE_DOWN } GuiKey;
#define GUI_TEXT_MAX 4

typedef enum { GUI_LOAD_NONE, GUI_LOAD_DROP, GUI_LOAD_GAMES, GUI_LOAD_KEYS } GuiLoadView;
typedef struct {
    int selected;  
    int active;    
    bool activated;
    GuiLoadView view;
    bool running, quit;
    int text_size;  
} GuiMenu;

typedef enum { GUI_MENU_STAY, GUI_MENU_SETTINGS, GUI_MENU_LOG, GUI_MENU_DATA, GUI_MENU_MOBILE } GuiMenuDo;
GuiMenuDo gui_menu_key(GuiMenu* menu, GuiKey key);

enum { GUI_CONFIRM_KEEP, GUI_CONFIRM_ABORT };
typedef enum {
    GUI_ASK_ABORT,    
    GUI_ASK_EXIT,     
    GUI_ASK_DEEP,     
    GUI_ASK_OUTSIDE,  
    GUI_ASK_ROOM      
} GuiAsk;
typedef struct {
    bool shown;
    GuiAsk ask;
    int selected;
    bool twice;  
    bool armed;  
} GuiConfirm;
void gui_confirm_open(GuiConfirm* confirm, GuiAsk ask, bool twice);

bool gui_confirm_key(GuiConfirm* confirm, GuiKey key);

void gui_confirm_focus(GuiConfirm* confirm, int choice);
const char* gui_setting_description(const GuiMenu* menu);

void gui_size_text(char* out, size_t size, uint64_t bytes);

bool gui_room(uint64_t dump, bool copy, uint64_t work_free, uint64_t games_free, bool shared, uint64_t* need,
              uint64_t* left);

void gui_usage_text(char* out, size_t size, bool available, uint64_t memory_bytes, double cpu_percent, int cores);

int gui_slide(int over, uint64_t ms, uint32_t step_ms);

size_t gui_wrap(const char* s, int width);

typedef struct {
    size_t count, page, page_size;
    size_t head;      
    size_t selected;  
    size_t active;    
    bool activated;
} GuiBrowser;

typedef enum { GUI_BROWSER_NONE, GUI_BROWSER_BACK, GUI_BROWSER_HEAD, GUI_BROWSER_OPEN } GuiBrowserAction;
size_t gui_browser_pages(const GuiBrowser* browser);
size_t gui_browser_items(const GuiBrowser* browser);
void gui_browser_resize(GuiBrowser* browser, size_t page_size);
GuiBrowserAction gui_browser_key(GuiBrowser* browser, GuiKey key);

#define GUI_MAX_COLS 240
#define GUI_MAX_ROWS 100
typedef struct {
    unsigned char cells[GUI_MAX_ROWS][GUI_MAX_COLS];
    int cols, rows, row, col, escape, utf8_left;
    uint32_t codepoint, utf8_min;
} GuiConsole;
void gui_console_init(GuiConsole* console, int cols, int rows);
void gui_console_resize(GuiConsole* console, int cols, int rows);
void gui_console_feed(GuiConsole* console, const char* data, size_t size);

char* gui_drop_path(const char* text, bool text_drop);

char* gui_document_path(const char* uri);

int gui_keys_text(const char* data, size_t size);
#endif
