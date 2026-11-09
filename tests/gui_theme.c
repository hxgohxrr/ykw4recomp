/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/utilities/common.h"
#include "gui_theme_colors.h"
#include "gui/steps.h"  
#include "gui/theme.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char* const format_keys[] = {"theme/format",
                                          "theme/name",
                                          "theme/author",
                                          "theme/layout",
                                          "theme/base",
                                          "colors/page",
                                          "colors/ink",
                                          "colors/soft",
                                          "colors/focus",
                                          "colors/active",
                                          "colors/failure",
                                          "colors/dim",
                                          "colors/warning",
                                          "colors/added",
                                          "head/style",
                                          "head/rule",
                                          "head/place",
                                          "head/size",
                                          "head/shadow",
                                          "head/menu",
                                          "head/games",
                                          "head/keys",
                                          "head/compiling",
                                          "head/loading",
                                          "body/marker",
                                          "body/focus",
                                          "body/panels",
                                          "body/sizing",
                                          "body/text",
                                          "body/cursor",
                                          "backdrop/grid",
                                          "tips/place",
                                          "tips/box",
                                          "tips/wrap",
                                          "footer/style",
                                          "footer/rule",
                                          "footer/box",
                                          "run/layout",
                                          "run/page",
                                          "run/ink",
                                          "run/soft",
                                          "run/dim",
                                          "run/failure",
                                          "run/warning",
                                          "run/texture",
                                          "run/bar",
                                          "run/detailed",
                                          "run/collapse",
                                          "run/ring",
                                          "run/progress",
                                          "prompt/align",
                                          "prompt/open",
                                          "prompt/close",
                                          "prompt/blink",
                                          "prompt/keys",
                                          "prompt/done",
                                          "prompt/failed",
                                          "prompt/aborting",
                                          "transitions/fade_in_ms",
                                          "transitions/fade_out_ms",
                                          "transitions/line_in",
                                          "transitions/line_in_ms",
                                          "transitions/typing_ms",
                                          "transitions/typing",
                                          "transitions/blink_ms",
                                          "transitions/scroll_ms",
                                          "splash/page",
                                          "splash/ms",
                                          "splash/chance"};

static const char* const eret_keys[] = {"colors/texture", "header/ring_side", "loading/title"};

static int same(GuiRgb a, GuiRgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int rgb_hex(GuiRgb a, unsigned long rgb) {
    return a.r == (rgb >> 16 & 0xFF) && a.g == (rgb >> 8 & 0xFF) && a.b == (rgb & 0xFF);
}
static int listed_in(const char* const* list, size_t count, const char* key) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (!strcmp(list[i], key)) return 1;
    return 0;
}

static int key_lines(const char* text, size_t size, char* keys, size_t keys_size) {
    char section[32] = "", line[256];
    size_t at = 0, used = 0, n;
    int count = 0;
    while (at < size) {
        char *s, *e;
        n = 0;
        while (at < size && text[at] != '\n') {
            if (n + 1 < sizeof line) line[n++] = text[at];
            ++at;
        }
        ++at;
        line[n] = 0;
        for (s = line; *s == ' ' || *s == '\t'; ++s) {}
        if (!*s || *s == ';' || *s == '#') continue;
        if (*s == '[') {
            e = strchr(s, ']');
            snprintf(section, sizeof section, "%.*s", e ? (int)(e - s - 1) : 0, s + 1);
            continue;
        }
        e = strchr(s, '=');
        if (!e) continue;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
        if (used + strlen(section) + (size_t)(e - s) + 2 > keys_size) break;
        used += (size_t)sprintf(keys + used, "%s/%.*s", section, (int)(e - s), s) + 1;
        ++count;
    }
    return count;
}
static int rgb_is(GuiRgb a, int r, int g, int b) { return a.r == r && a.g == g && a.b == b; }
static void parse(GuiTheme* theme, const char* text) { assert(gui_theme_parse(theme, "mine", text, strlen(text))); }
static void write(const char* dir, const char* name, const char* text) {
    char* folder = snx_path_join(dir, name);
    char* file = snx_path_join(folder, "theme.ini");
    assert(!snx_mkdirs(folder));
    if (text) assert(!snx_write_file(file, text, strlen(text)));
    free(file);
    free(folder);
}

int main(void) {
    static const char* const ids[] = {"default", "yorha"};
    const char* shipped[] = {SNX_GUI_THEMES_DIR};
    char* missing;
    GuiThemes themes;
    const char *dirs[3], *none[1];
    char* second;
    char* dir;
    char longer[200];
    const GuiTheme* b;
    GuiTheme theme;
    size_t i;
    const GuiTheme *def = gui_theme_builtin("default"), *yorha = gui_theme_builtin("yorha");
    const GuiTheme* loading = yorha;  
    assert(def && yorha && !gui_theme_builtin("yorha-loading") && !gui_theme_builtin("nothing") &&
           !gui_theme_builtin(NULL));

    for (i = 0; gui_theme_keys[i].section; ++i) {
        snprintf(longer, sizeof longer, "%s/%s", gui_theme_keys[i].section, gui_theme_keys[i].key);
        assert(listed_in(format_keys, sizeof format_keys / sizeof *format_keys, longer));
    }
    assert(i == sizeof format_keys / sizeof *format_keys - 3 && !gui_theme_key("theme", "format") &&
           !gui_theme_key("theme", "layout") && !gui_theme_key("theme", "base"));
    assert(rgb_is(yorha->run_colors.page, 0x14, 0x14, 0x16) && !same(yorha->run_colors.page, yorha->page));
    assert(same(def->run_colors.page, def->page) && same(def->run_colors.ink, def->ink));
    assert(def->run == GUI_RUN_CONSOLE && !def->fade_in_ms && !def->fade_out_ms && def->line_in == GUI_LINE_NONE);
    assert(loading->run == GUI_RUN_STEPS && loading->ring == GUI_RING_RIGHT && loading->texture && !loading->bar);
    assert(loading->line_in == GUI_LINE_TYPING && loading->typing_ms == 5 && loading->fade_out_ms == 500);

    assert(def->progress == GUI_PROGRESS_AMOUNT && yorha->progress == GUI_PROGRESS_PERCENT);
    theme = *def;
    parse(&theme, "[run]\nprogress = time\n");
    assert(theme.progress == GUI_PROGRESS_TIME);
    assert(loading->typing_sync && !def->typing_sync);   
    assert(yorha->scroll_ms == 120 && !def->scroll_ms);  
    assert(loading->detailed && !loading->collapse && !def->detailed && !def->collapse);
    for (b = def; b; b = b == def ? loading : NULL) {
        const GuiPrompt* p = &b->prompt;
        assert(p->align == GUI_ALIGN_CENTER && !strcmp(p->open, "[ ") && !strcmp(p->close, " ]") && p->blink);
        assert(p->lines[GUI_PROMPT_KEYS] && !p->lines[GUI_PROMPT_DONE] && !p->lines[GUI_PROMPT_FAILED] &&
               !p->lines[GUI_PROMPT_ABORTING]);
    }
    assert(!strcmp(loading->title[GUI_SCREEN_LOADING], "BOOTING"));
    assert(def->head == GUI_STYLE_PLAIN && !def->focus_bar && !strcmp(def->marker, "- ") &&
           !strcmp(def->title[GUI_SCREEN_MENU], yorha->title[GUI_SCREEN_MENU]));
    assert(yorha->head == GUI_STYLE_PLAIN && yorha->focus_bar && yorha->title[GUI_SCREEN_COMPILING][0]);

    assert(def->panels == GUI_PANELS_OUTLINE && !def->cursor[0] && yorha->panels == GUI_PANELS_FILL &&
           !strcmp(yorha->cursor, "\xe2\x96\xba "));
    assert(def->head_place == GUI_HEAD_TABS && yorha->head_place == GUI_HEAD_TABS);
    assert(def->title_size == 1 && !def->title_shadow && yorha->title_size == 2 && !yorha->title_shadow);
    assert(!def->grid && yorha->grid && def->tips_place == GUI_TIPS_TOP && yorha->tips_place == GUI_TIPS_TOP);
    assert(!def->tips_box && !def->footer_box && yorha->tips_box && yorha->footer_box);
    assert(def->footer == GUI_STYLE_PLAIN && yorha->footer == GUI_STYLE_BAR && yorha->head_rule && !def->head_rule);
    assert(!gui_theme_key("body", "layout") && !gui_theme_key("head", "tabs") && !gui_theme_key("frame", "edges"));

    parse(&theme, "");  
    assert(same(theme.ink, def->ink) && !strcmp(theme.id, "mine") && !strcmp(theme.name, "mine"));


    parse(&theme, "; a comment\r\n[colors]\r\npage = #0a0B0c\r\nink = #12345\r\nsoft = # 12 34\r\nfocus = #GG0000\r\n"
                  "[theme]\r\nname = Mine\r\nbase = yorha\r\n# another\r\n[head]\r\nstyle = none\r\nrule = maybe\r\n"
                  "loading = \"  BOOT  \"\r\nmenu =\r\n[footer]\r\nstyle = plain\r\nrule = true\r\n"
                  "[body]\r\nmarker = \"* \"\r\nfocus = color\r\n[nowhere]\r\nink = #FFFFFF\r\nstray line\r\n");
    assert(!strcmp(theme.name, "Mine") && !strcmp(theme.id, "mine"));
    assert(theme.scroll_ms == yorha->scroll_ms);
    assert(rgb_is(theme.page, 0x0A, 0x0B, 0x0C));
    assert(same(theme.ink, yorha->ink) && same(theme.soft, yorha->soft) && same(theme.focus, yorha->focus));
    assert(theme.head == yorha->head && theme.head_rule == yorha->head_rule);  
    assert(theme.footer == GUI_STYLE_PLAIN && theme.footer_rule && !theme.focus_bar && !strcmp(theme.marker, "* "));
    assert(!strcmp(theme.title[GUI_SCREEN_LOADING], "  BOOT  ") && !theme.title[GUI_SCREEN_MENU][0]);
    assert(!strcmp(theme.title[GUI_SCREEN_COMPILING], yorha->title[GUI_SCREEN_COMPILING]));


    parse(&theme,
          "[theme]\nbase = yorha\n[run]\nlayout = console\nring = left\ntexture = false\nbar = true\npage = #050505\n"
          "detailed = false\ncollapse = true\n[transitions]\nline_in = slide\nline_in_ms = 300\nfade_in_ms = 10001\n"
          "fade_out_ms = 5s\ntyping_ms = 0\nblink_ms = -1\n[colors]\ndim = #010203\n");
    assert(theme.run == GUI_RUN_CONSOLE && theme.ring == GUI_RING_LEFT && !theme.texture && theme.bar);
    assert(!theme.detailed && theme.collapse);
    assert(theme.line_in == GUI_LINE_SLIDE && theme.line_in_ms == 300 && theme.typing_ms == 0);
    assert(theme.fade_in_ms == loading->fade_in_ms && theme.fade_out_ms == loading->fade_out_ms);
    assert(theme.blink_ms == loading->blink_ms && rgb_is(theme.dim, 1, 2, 3));
    assert(rgb_is(theme.run_colors.page, 5, 5, 5) && same(theme.run_colors.ink, yorha->run_colors.ink));
    assert(same(theme.page, yorha->page));


    parse(&theme, "[theme]\nformat = 2\nname = From rEden\nauthor = rEden\nlayout = menu\n[colors]\ntexture = true\n"
                  "warning = #D9B24A\n[header]\nring_side = right-near-text\n[transitions]\ntyping = sync\n"
                  "[loading]\ntitle = WAKING\nheader = SYSTEM  /  LOADING\n[plain]\nstage_build = %1 / %2\n"
                  "[sizes]\ntitle_scale = 0.75\n");
    assert(same(theme.page, yorha->page) && theme.format == 2 && !strcmp(theme.author, "rEden"));
    assert(theme.texture && theme.ring == GUI_RING_NEAR_TEXT && theme.typing_sync);
    assert(rgb_is(theme.warning, 0xD9, 0xB2, 0x4A) && !strcmp(theme.title[GUI_SCREEN_LOADING], "WAKING"));

    parse(&theme, "[theme]\nlayout = loading\n[colors]\npage = #101010\nink = #F0F0F0\ntexture = false\n");
    assert(rgb_is(theme.run_colors.page, 16, 16, 16) && rgb_is(theme.run_colors.ink, 240, 240, 240));
    assert(same(theme.page, yorha->page) && same(theme.ink, yorha->ink) && !theme.texture &&
           theme.run == GUI_RUN_STEPS);
    parse(&theme, "[theme]\nbase = yorha\nlayout = loading\n[colors]\npage = #101010\n");  
    assert(rgb_is(theme.page, 16, 16, 16) && same(theme.run_colors.page, yorha->run_colors.page));
    parse(&theme, "[theme]\nbase = default\nlayout = menu\nformat = 0\n");  
    assert(same(theme.page, def->page) && theme.format == GUI_THEME_FORMAT);
    parse(&theme, "[header]\nring_side = nowhere\n[run]\nring = right-near-text\n[transitions]\ntyping = later\n");
    assert(theme.ring == GUI_RING_NEAR_TEXT && !theme.typing_sync);


    parse(&theme, "[prompt]\nalign = right\nopen = \"<< \"\nclose = \"\"\nblink = false\nfailed = true\n"
                  "aborting = true\nkeys = maybe\ndone = false\nnowhere = true\n");
    assert(theme.prompt.align == GUI_ALIGN_RIGHT && !strcmp(theme.prompt.open, "<< ") && !theme.prompt.close[0]);
    assert(!theme.prompt.blink && theme.prompt.lines[GUI_PROMPT_FAILED] && theme.prompt.lines[GUI_PROMPT_ABORTING]);
    assert(theme.prompt.lines[GUI_PROMPT_KEYS] && !theme.prompt.lines[GUI_PROMPT_DONE]);
    parse(&theme, "[prompt]\nalign = middle\nopen = \"[[[[[[[[[[[[[[[[[[[[\"\nkeys = false\n");
    assert(theme.prompt.align == GUI_ALIGN_CENTER && !strcmp(theme.prompt.open, "[ ") &&
           !theme.prompt.lines[GUI_PROMPT_KEYS]);
    parse(&theme, "[prompt]\nalign = left\n");
    assert(theme.prompt.align == GUI_ALIGN_LEFT);


    parse(&theme, "[theme]\nformat = 1\nbase = default\n[colors]\npage = #102030\n");
    assert(theme.panels == GUI_PANELS_OUTLINE && theme.head_place == GUI_HEAD_TABS &&
           rgb_is(theme.page, 0x10, 0x20, 0x30));
    parse(&theme, "[theme]\nbase = yorha\n[body]\ncursor = \"> \"\n");
    assert(theme.panels == GUI_PANELS_FILL && !strcmp(theme.cursor, "> "));
    parse(&theme, "[theme]\nbase = yorha\n[body]\npanels = outline\n[head]\nplace = below\n[tips]\nplace = bottom\n"
                  "box = false\n[footer]\nbox = false\nstyle = none\n");
    assert(theme.panels == GUI_PANELS_OUTLINE && theme.head_place == GUI_HEAD_BELOW &&
           theme.tips_place == GUI_TIPS_BOTTOM);
    assert(!theme.tips_box && !theme.footer_box && theme.footer == GUI_STYLE_NONE);
    parse(&theme, "[body]\npanels = round\n[head]\nplace = left\n[tips]\nplace = side\n");
    assert(theme.panels == def->panels && theme.head_place == def->head_place && theme.tips_place == def->tips_place);


    memset(longer, 0, sizeof longer);
    strcpy(longer, "[theme]\nbase = elsewhere\n[body]\nmarker = ");
    memset(longer + strlen(longer), 'x', 40);
    parse(&theme, longer);
    assert(!strcmp(theme.marker, def->marker) && same(theme.page, def->page));

    dir = snx_temp_dir(NULL);
    second = snx_temp_dir(NULL);
    assert(dir && second);
    write(dir, "alpha", "[theme]\nbase = yorha\nname = Alpha\n");
    write(dir, "beta", NULL);  
    write(dir, ".hidden", "[theme]\n");
    write(dir, "yorha", "[theme]\nbase = yorha\n[colors]\npage = #FFFFFF\n");  
    write(second, "alpha", "[theme]\nname = Alpha too\n");                     
    write(second, "gamma", "[theme]\nname = Gamma\n");
    dirs[0] = dir;
    dirs[1] = NULL;
    dirs[2] = second;
    memset(&themes, 0, sizeof themes);
    gui_themes_scan(&themes, dirs, 3);
    assert(themes.count == 4 && !strcmp(themes.ids[0], "default") && !strcmp(themes.ids[1], "yorha") &&
           !strcmp(themes.ids[2], "alpha") && !strcmp(themes.ids[3], "gamma"));
    gui_theme_load(&theme, dirs, 3, "alpha");
    assert(!strcmp(theme.name, "Alpha") && same(theme.page, yorha->page));
    gui_theme_load(&theme, dirs, 3, "gamma");
    assert(!strcmp(theme.name, "Gamma") && same(theme.page, def->page));
    gui_theme_load(&theme, dirs, 3, "yorha");
    assert(rgb_is(theme.page, 255, 255, 255) && same(theme.ink, yorha->ink));
    gui_theme_load(&theme, dirs, 3, "beta");
    assert(!strcmp(theme.id, "default"));
    gui_themes_free(&themes);
    missing = snx_path_join(dir, "missing");
    none[0] = missing;
    gui_themes_scan(&themes, none, 1);  
    assert(themes.count == 2);
    gui_themes_free(&themes);
    free(missing);
    assert(!snx_remove_tree(dir) && !snx_remove_tree(second));
    free(dir);
    free(second);


    for (i = 0; i < sizeof ids / sizeof *ids; ++i) {
        int l;
        int s;
        const GuiTheme* b = gui_theme_builtin(ids[i]);
        GuiTheme f;
        gui_theme_load(&f, shipped, 1, ids[i]);
        assert(!strcmp(f.id, b->id) && !strcmp(f.name, b->name) && !strcmp(f.marker, b->marker));
        assert(same(f.page, b->page) && same(f.ink, b->ink) && same(f.soft, b->soft) && same(f.focus, b->focus));
        assert(same(f.active, b->active) && same(f.failure, b->failure) && same(f.dim, b->dim));
        assert(f.head == b->head && f.footer == b->footer && f.head_rule == b->head_rule);
        assert(f.footer_rule == b->footer_rule && f.focus_bar == b->focus_bar && f.run == b->run);
        assert(f.texture == b->texture && f.bar == b->bar && f.ring == b->ring && f.line_in == b->line_in);
        assert(f.fade_in_ms == b->fade_in_ms && f.fade_out_ms == b->fade_out_ms && f.line_in_ms == b->line_in_ms);
        assert(f.typing_ms == b->typing_ms && f.blink_ms == b->blink_ms && f.typing_sync == b->typing_sync);
        assert(f.detailed == b->detailed && f.collapse == b->collapse);
        assert(f.prompt.align == b->prompt.align && !strcmp(f.prompt.open, b->prompt.open));
        assert(!strcmp(f.prompt.close, b->prompt.close) && f.prompt.blink == b->prompt.blink);
        for (l = 0; l < GUI_PROMPT_COUNT; ++l) assert(f.prompt.lines[l] == b->prompt.lines[l]);
        assert(f.format == GUI_THEME_FORMAT && b->format == GUI_THEME_FORMAT && !strcmp(f.author, b->author));
        assert(same(f.warning, b->warning) && same(f.added, b->added) && same(f.run_colors.page, b->run_colors.page));
        assert(same(f.run_colors.ink, b->run_colors.ink) && same(f.run_colors.soft, b->run_colors.soft));
        assert(same(f.run_colors.dim, b->run_colors.dim) && same(f.run_colors.failure, b->run_colors.failure));
        assert(same(f.run_colors.warning, b->run_colors.warning));
        for (s = 0; s < GUI_SCREEN_COUNT; ++s) assert(!strcmp(f.title[s], b->title[s]));
        assert(f.scroll_ms == b->scroll_ms && same(f.splash_page, b->splash_page) && f.splash_ms == b->splash_ms);
        assert(f.splash_chance == b->splash_chance);
    }

    for (i = 0; i < sizeof ids / sizeof *ids; ++i) {
        static char keys[8192];
        size_t k, size = 0;
        char* folder = snx_path_join(SNX_GUI_THEMES_DIR, ids[i]);
        char* file = snx_path_join(folder, "theme.ini");
        char* text = (char*)snx_read_file(file, &size);
        int n, left, found;
        const char* key;
        assert(text);
        n = key_lines(text, size, keys, sizeof keys);
        for (k = 0; k < sizeof format_keys / sizeof *format_keys; ++k) {
            for (found = 0, left = n, key = keys; left-- > 0 && !found; key += strlen(key) + 1)
                found = !strcmp(key, format_keys[k]);
            if (!found) printf("%s lacks %s (section/key)\n", file, format_keys[k]);
            assert(found);
        }
        free(text);
        free(file);
        free(folder);
    }

    def = gui_theme_builtin("default");
    yorha = gui_theme_builtin("yorha");
    assert(rgb_hex(def->page, SNX_GUI_THEME_DEFAULT_PAGE) && rgb_hex(def->ink, SNX_GUI_THEME_DEFAULT_INK));
    assert(rgb_hex(def->soft, SNX_GUI_THEME_DEFAULT_SOFT) && rgb_hex(def->focus, SNX_GUI_THEME_DEFAULT_FOCUS));
    assert(rgb_hex(def->active, SNX_GUI_THEME_DEFAULT_ACTIVE) && rgb_hex(def->failure, SNX_GUI_THEME_DEFAULT_FAILURE));
    assert(rgb_hex(def->dim, SNX_GUI_THEME_DEFAULT_DIM) && rgb_hex(def->warning, SNX_GUI_THEME_DEFAULT_WARNING));
    assert(rgb_hex(def->run_colors.page, SNX_GUI_THEME_DEFAULT_RUN_PAGE));
    assert(rgb_hex(def->run_colors.ink, SNX_GUI_THEME_DEFAULT_RUN_INK));
    assert(rgb_hex(def->run_colors.soft, SNX_GUI_THEME_DEFAULT_RUN_SOFT));
    assert(rgb_hex(def->run_colors.dim, SNX_GUI_THEME_DEFAULT_RUN_DIM));
    assert(rgb_hex(def->run_colors.failure, SNX_GUI_THEME_DEFAULT_RUN_FAILURE));
    assert(rgb_hex(def->run_colors.warning, SNX_GUI_THEME_DEFAULT_RUN_WARNING));
    assert(rgb_hex(def->splash_page, SNX_GUI_THEME_DEFAULT_SPLASH_PAGE));
    assert(rgb_hex(yorha->page, SNX_GUI_THEME_YORHA_PAGE) && rgb_hex(yorha->ink, SNX_GUI_THEME_YORHA_INK));
    assert(rgb_hex(yorha->soft, SNX_GUI_THEME_YORHA_SOFT) && rgb_hex(yorha->focus, SNX_GUI_THEME_YORHA_FOCUS));
    assert(rgb_hex(yorha->active, SNX_GUI_THEME_YORHA_ACTIVE) && rgb_hex(yorha->failure, SNX_GUI_THEME_YORHA_FAILURE));
    assert(rgb_hex(yorha->dim, SNX_GUI_THEME_YORHA_DIM) && rgb_hex(yorha->warning, SNX_GUI_THEME_YORHA_WARNING));
    assert(rgb_hex(yorha->run_colors.page, SNX_GUI_THEME_YORHA_RUN_PAGE));
    assert(rgb_hex(yorha->run_colors.ink, SNX_GUI_THEME_YORHA_RUN_INK));
    assert(rgb_hex(yorha->run_colors.soft, SNX_GUI_THEME_YORHA_RUN_SOFT));
    assert(rgb_hex(yorha->run_colors.dim, SNX_GUI_THEME_YORHA_RUN_DIM));
    assert(rgb_hex(yorha->run_colors.failure, SNX_GUI_THEME_YORHA_RUN_FAILURE));
    assert(rgb_hex(yorha->run_colors.warning, SNX_GUI_THEME_YORHA_RUN_WARNING));
    assert(rgb_hex(yorha->splash_page, SNX_GUI_THEME_YORHA_SPLASH_PAGE));

    {
        static char keys[8192];
        size_t size = 0;
        char* doc = (char*)snx_read_file(SNX_GUI_DOCS, &size);
        char *from, *to;
        const char* key;
        int n;
        assert(doc);

        from = strstr(doc, "\n## Themes");
        from = from ? strstr(from, "```ini") : NULL;
        from = from ? strchr(from, '\n') : NULL;
        to = from ? strstr(from, "\n```") : NULL;
        assert(from && to);
        ++from;
        assert(gui_theme_parse(&theme, "doc", from, (size_t)(to - from)) && !strcmp(theme.name, "Night ink"));
        n = key_lines(from, (size_t)(to - from), keys, sizeof keys);
        assert(n > 40);
        for (key = keys; n-- > 0; key += strlen(key) + 1) {
            int known = listed_in(format_keys, sizeof format_keys / sizeof *format_keys, key) ||
                        listed_in(eret_keys, sizeof eret_keys / sizeof *eret_keys, key);
            if (!known) printf("docs/GUI.md's example has %s (section/key), which theme format 1 lacks\n", key);
            assert(known);
        }
        free(doc);
    }

    parse(&theme, "[theme]\nbase = default\n[transitions]\nscroll_ms = 40\n");
    assert(theme.scroll_ms == 40);

    parse(&theme, "[theme]\nbase = yorha\n[splash]\npage = #102030\nms = 0\nchance = 300\n");
    assert(rgb_is(theme.splash_page, 0x10, 0x20, 0x30) && !theme.splash_ms && theme.splash_chance == 100);
    assert(rgb_is(yorha->splash_page, 0xDA, 0xD6, 0xBE) && yorha->splash_chance == 25 && yorha->splash_ms);
    assert(rgb_is(def->splash_page, 0, 0, 0) && def->splash_chance == 100 && def->splash_ms);
    puts("GUI themes: built-ins, theme.ini over its base, themes/ folders passed");
    return 0;
}
