/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */
#ifndef SNX_GUI_THEME_H
#define SNX_GUI_THEME_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} GuiRgb;
typedef enum { GUI_STYLE_PLAIN, GUI_STYLE_BAR, GUI_STYLE_NONE } GuiStyle;

typedef enum { GUI_RUN_CONSOLE, GUI_RUN_STEPS } GuiRunLayout;
typedef enum { GUI_LINE_NONE, GUI_LINE_TYPING, GUI_LINE_SLIDE, GUI_LINE_FADE } GuiLineIn;
typedef enum { GUI_RING_NONE, GUI_RING_LEFT, GUI_RING_RIGHT, GUI_RING_NEAR_TEXT } GuiRing;
typedef enum { GUI_ALIGN_LEFT, GUI_ALIGN_CENTER, GUI_ALIGN_RIGHT } GuiAlign;
typedef enum { GUI_PANELS_OUTLINE, GUI_PANELS_FILL } GuiPanels;  

typedef enum { GUI_SIZING_FIXED, GUI_SIZING_ADAPTIVE } GuiSizing;

typedef enum { GUI_TEXT_WRAP, GUI_TEXT_SLIDE } GuiTextFlow;

typedef enum { GUI_HEAD_TABS, GUI_HEAD_BELOW } GuiHeadPlace;

typedef enum { GUI_TIPS_BOTTOM, GUI_TIPS_TOP } GuiTipsPlace;

typedef enum {
    GUI_PROMPT_KEYS,
    GUI_PROMPT_DONE,
    GUI_PROMPT_FAILED,
    GUI_PROMPT_ABORTING,
    GUI_PROMPT_COUNT
} GuiPromptLine;

typedef struct {
    GuiAlign align;
    char open[16], close[16];
    int blink;
    int lines[GUI_PROMPT_COUNT];
} GuiPrompt;

#define GUI_THEME_FORMAT 1
typedef enum {
    GUI_SCREEN_MENU,
    GUI_SCREEN_GAMES,
    GUI_SCREEN_KEYS,
    GUI_SCREEN_COMPILING,
    GUI_SCREEN_LOADING,
    GUI_SCREEN_COUNT
} GuiScreen;
typedef struct {
    GuiRgb page, ink, soft, dim, failure, warning;
} GuiPalette;
typedef struct {
    char id[64], name[64], author[64];
    uint32_t format;
    GuiRgb page, ink, soft, focus, active, failure, dim, warning, added;
    GuiStyle head, footer;  
    int head_rule, footer_rule;
    GuiHeadPlace head_place;
    uint32_t title_size;  
    int title_shadow;
    int focus_bar;  
    char marker[16];
    GuiPanels panels;
    GuiSizing sizing;
    GuiTextFlow text;
    char cursor[16];  
    int grid;         
    GuiTipsPlace tips_place;
    int tips_box, footer_box;  
    int tips_wrap;             
    char title[GUI_SCREEN_COUNT][64];
    GuiRunLayout run;
    GuiPalette run_colors;   
    int texture, bar;        
    int detailed, collapse;  
    GuiRing ring;
    int progress;  
    GuiLineIn line_in;
    int typing_sync;  
    GuiPrompt prompt;
    uint32_t fade_in_ms, fade_out_ms, line_in_ms, typing_ms, blink_ms;
    uint32_t scroll_ms;  

    GuiRgb splash_page;
    uint32_t splash_ms, splash_chance;
} GuiTheme;
typedef struct {
    char (*ids)[64];  
    size_t count;
} GuiThemes;



typedef enum { GUI_KEY_TEXT, GUI_KEY_COLOUR, GUI_KEY_FLAG, GUI_KEY_NUMBER, GUI_KEY_WORD, GUI_KEY_ACTION } GuiKeyType;
typedef struct {
    const char* section;
    const char* key;
    GuiKeyType type;
    size_t at, size;           
    const char* const* words;  
    uint32_t max;              
    const char* about;
} GuiThemeKey;
extern const GuiThemeKey gui_theme_keys[];  

const char* gui_theme_section_about(const char* section);
const GuiThemeKey* gui_theme_key(const char* section, const char* key);

int gui_theme_set(void* object, const GuiThemeKey* key, const char* value);

void gui_theme_get(const void* object, const GuiThemeKey* key, char* out, size_t size);


typedef struct {
    char base[64], layout[16];
    int ring_side, loading_title, colors_texture;
    char* others;
} GuiThemeFile;

int gui_theme_file_read(GuiThemeFile* file, const char* text, size_t size);
void gui_theme_file_free(GuiThemeFile* file);

char* gui_theme_write(const GuiTheme* theme, const GuiThemeFile* file);


const GuiTheme* gui_theme_builtin(const char* id);

const GuiTheme* gui_splash_theme(const GuiTheme* theme, int roll);

int gui_theme_parse(GuiTheme* theme, const char* id, const char* text, size_t size);

char* gui_theme_text(const char* const* dirs, size_t count, const char* id, size_t* size);

void gui_theme_load(GuiTheme* theme, const char* const* dirs, size_t count, const char* id);

void gui_themes_scan(GuiThemes* themes, const char* const* dirs, size_t count);
void gui_themes_free(GuiThemes* themes);
#endif
