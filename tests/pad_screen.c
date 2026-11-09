/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#include "common/utilities/pad_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { A, B, X, Y };


static int over(const SnxPadShape* a, const SnxPadShape* b) {
    const float e = 0.01f;
    return a->x < b->x + b->w - e && b->x < a->x + a->w - e && a->y < b->y + b->h - e && b->y < a->y + a->h - e;
}
static int inside(const SnxPadShape* in, const SnxPadShape* out) {
    return in->x >= out->x && in->y >= out->y && in->x + in->w <= out->x + out->w && in->y + in->h <= out->y + out->h;
}

static void tidy(const SnxPadDrawing* d, float x, float y, float w, float h) {
    int i, j;
    assert(d->n > 0);
    for (i = 0; i < d->n; ++i) {
        const SnxPadShape* p = &d->shape[i];
        assert(p->x >= x && p->y >= y && p->x + p->w <= x + w + 0.01f && p->y + p->h <= y + h + 0.01f);
        for (j = 0; j < i; ++j) {
            const SnxPadShape* q = &d->shape[j];
            int text_in =
                (p->kind == SNX_PAD_SHAPE_TEXT && inside(p, q)) || (q->kind == SNX_PAD_SHAPE_TEXT && inside(q, p));
            if (over(p, q) && !text_in) {
                fprintf(stderr, "shape %d (kind %d id %d) over shape %d (kind %d id %d)\n", i, p->kind, p->id, j,
                        q->kind, q->id);
                assert(0);
            }
        }
    }
}
static int has_text(const SnxPadDrawing* d, const char* s) {
    int i;
    for (i = 0; i < d->n; ++i)
        if (d->shape[i].kind == SNX_PAD_SHAPE_TEXT && strstr(d->shape[i].text, s)) return 1;
    return 0;
}

int main(void) {
    SnxPadScreen s;
    SnxPadDrawing d;
    SnxPadMap map;
    static const int sizes[][2] = {{1280, 800}, {2560, 1440}, {800, 480}, {640, 400}, {480, 800}, {360, 640}};
    size_t k;


    snx_pad_screen_start(&s);
    assert(s.focus == SNX_PAD_LT && !s.swap_ab);
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_SOUTH] == B && map.bit[SNX_PAD_EAST] == A && map.bit[SNX_PAD_NORTH] == X);

    s.swap_ab = 1;
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_SOUTH] == A && map.bit[SNX_PAD_EAST] == B && map.bit[SNX_PAD_NORTH] == X);
    s.swap_ab = 0;


    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_UP) == SNX_PAD_DID_NOTHING && s.focus == SNX_PAD_START);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_DOWN) == SNX_PAD_DID_NOTHING && s.focus == SNX_PAD_LT);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_DOWN) == SNX_PAD_DID_NOTHING && s.focus == SNX_PAD_LB);

    s.focus = SNX_PAD_SOUTH;
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_RIGHT) == SNX_PAD_DID_MAP);
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_SOUTH] == X && s.own.bit[SNX_PAD_SOUTH] == X && map.bit[SNX_PAD_EAST] == A);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_LEFT) == SNX_PAD_DID_MAP);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_LEFT) == SNX_PAD_DID_MAP);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_LEFT) == SNX_PAD_DID_MAP);
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_SOUTH] == SNX_PAD_NONE);  

    snx_pad_screen_start(&s);
    s.swap_ab = 1;
    s.focus = SNX_PAD_EAST;  
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_RIGHT) == SNX_PAD_DID_MAP);
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_EAST] == X && s.own.bit[SNX_PAD_EAST] == X && map.bit[SNX_PAD_SOUTH] == A);
    s.focus = SNX_PAD_START;
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_LEFT) == SNX_PAD_DID_MAP);  
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_LEFT) == SNX_PAD_DID_MAP);  
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_START] == 8 && s.own.bit[SNX_PAD_SOUTH] == B);  
    snx_pad_screen_start(&s);

    s.focus = SNX_PAD_SOUTH;
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_SELECT) == SNX_PAD_DID_NOTHING && !s.listening);
    snprintf(s.pad, sizeof s.pad, "Acer Gamepad");
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_SELECT) == SNX_PAD_DID_NOTHING && s.listening);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_DOWN) == SNX_PAD_DID_NOTHING && s.focus == SNX_PAD_SOUTH);
    assert(snx_pad_screen_press(&s, SNX_PAD_EAST) == SNX_PAD_DID_MAP && !s.listening);
    snx_pad_screen_map(&s, &map);
    assert(map.bit[SNX_PAD_SOUTH] == A && map.bit[SNX_PAD_EAST] == B && s.focus == SNX_PAD_EAST);
    assert(snx_pad_screen_press(&s, SNX_PAD_SOUTH) == SNX_PAD_DID_NOTHING);  

    snx_pad_screen_key(&s, SNX_PAD_KEY_SELECT);
    assert(s.listening && snx_pad_screen_key(&s, SNX_PAD_KEY_BACK) == SNX_PAD_DID_NOTHING && !s.listening);
    assert(snx_pad_screen_key(&s, SNX_PAD_KEY_BACK) == SNX_PAD_DID_LEAVE);


    assert(snx_pad_screen_tap(&s, SNX_PAD_NORTH) == SNX_PAD_DID_NOTHING && !s.listening && s.focus == SNX_PAD_NORTH);
    assert(snx_pad_screen_tap(&s, SNX_PAD_NORTH) == SNX_PAD_DID_NOTHING && s.listening);
    assert(snx_pad_screen_tap(&s, SNX_PAD_LT) == SNX_PAD_DID_NOTHING && s.listening);  
    s.listening = 0;
    assert(snx_pad_screen_tap(&s, -1) == SNX_PAD_DID_NOTHING && snx_pad_screen_tap(&s, 99) == SNX_PAD_DID_NOTHING);


    s.labels[0] = SNX_PAD_LABEL_CROSS;  
    s.labels[1] = SNX_PAD_LABEL_CIRCLE;
    s.labels[2] = SNX_PAD_LABEL_SQUARE;
    s.labels[3] = SNX_PAD_LABEL_TRIANGLE;
    for (k = 0; k < sizeof sizes / sizeof sizes[0]; ++k) {
        int scale, sym;
        for (sym = 0; sym < 2; ++sym)
            for (scale = 1; scale <= 2; ++scale) {
                float w = (float)sizes[k][0], h = (float)sizes[k][1], cw = 8.0f * (float)scale,
                      ch = 16.0f * (float)scale;
                int i, found = 0;
                s.symmetric = sym;
                s.focus = SNX_PAD_SOUTH;
                snx_pad_screen_lay(&s, 0, 0, w, h, cw, ch, &d);
                tidy(&d, 0, 0, w, h);
                assert(has_text(&d, "Cross sends") && !has_text(&d, "By position") && !has_text(&d, "By label"));
                for (i = 0; i < d.n; ++i)
                    if (d.shape[i].id == SNX_PAD_SOUTH && d.shape[i].kind == SNX_PAD_SHAPE_BOX) {
                        found = 1;
                        assert(d.shape[i].style == SNX_PAD_STYLE_FOCUS);
                        assert(snx_pad_screen_hit(&d, d.shape[i].x + 1, d.shape[i].y + 1) == SNX_PAD_SOUTH);
                    }
                assert(found);  
            }
    }

    s.held = 1u << SNX_PAD_RT;
    s.focus = SNX_PAD_LT;
    snx_pad_screen_lay(&s, 0, 0, 1280, 800, 16, 32, &d);
    {
        int i, lit = 0, named = 0, south = -1;
        for (i = 0; i < d.n; ++i) {
            const SnxPadShape* p = &d.shape[i];
            lit |= p->id == SNX_PAD_RT && p->style == SNX_PAD_STYLE_LIT;
            if (p->kind == SNX_PAD_SHAPE_TEXT && !strcmp(p->text, "Acer Gamepad")) named = 1;
            if (p->kind == SNX_PAD_SHAPE_TEXT && !strcmp(p->text, "Cross") && south < 0) south = i;
        }
        assert(lit && named && south >= 0 && south + 1 < d.n);
        assert(d.shape[south + 1].kind == SNX_PAD_SHAPE_TEXT && d.shape[south + 1].x > d.shape[south].x + 4 * 16);
    }

    s.pad[0] = 0;
    snprintf(s.note, sizeof s.note,
             "Arcade Stick has no SDL mapping: add a line for GUID "
             "03000000341200007856000000000000 to /home/player/.config/YKW4Recomp/config/"
             "gamecontrollerdb.txt");
    for (k = 0; k < sizeof sizes / sizeof sizes[0]; ++k) {
        char whole[sizeof s.note] = "";
        int i, title = 0;
        snx_pad_screen_lay(&s, 0, 0, (float)sizes[k][0], (float)sizes[k][1], 16, 32, &d);
        tidy(&d, 0, 0, (float)sizes[k][0], (float)sizes[k][1]);
        for (i = 0; i < d.n; ++i) {
            const SnxPadShape* p = &d.shape[i];
            if (p->kind == SNX_PAD_SHAPE_TEXT && !strcmp(p->text, "No controller")) title = 1;
            if (p->kind == SNX_PAD_SHAPE_TEXT && p->style == SNX_PAD_STYLE_NOTE) {
                if (whole[0] && whole[strlen(whole) - 1] != '/') strcat(whole, " ");
                strcat(whole, p->text);
            }
        }
        if (sizes[k][0] >= 1280) assert(title && !strcmp(whole, s.note));  
    }
    s.note[0] = 0;
    snx_pad_screen_lay(&s, 0, 0, 1280, 800, 16, 32, &d);
    assert(has_text(&d, "No controller") && has_text(&d, "Plug one in"));
    puts("pad_screen passed");
    return 0;
}
