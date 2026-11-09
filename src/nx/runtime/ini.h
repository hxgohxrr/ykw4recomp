/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright 2026 the ScarletNX contributors */

#ifndef SNX_INI_H
#define SNX_INI_H
#include <stddef.h>
#include <stdio.h>

enum {
    INI_INT,    
    INI_FLOAT,  
    INI_TEXT    
};
#define INI_STARTUP 1  
#define INI_IN_GAME 2  

typedef struct IniOpt IniOpt;
struct IniOpt {
    const char* key;   
    const char* def;   
    const char* help;  
    int type, flags;
    void* var;
    double lo, hi;

    void (*set)(const IniOpt* o, const char* v);

    const char* name;
    const char* words;
    int step, shift;
};

const IniOpt* ini_find(const IniOpt* table, const char* key);  
void ini_read(const IniOpt* o, const char* v);  

void ini_format(const IniOpt* o, char* out, size_t n);

void ini_word(const IniOpt* o, int v, char* out, size_t n);

int ini_step(const IniOpt* o, int v, int dir);


const char* ini_deprecated(const char* key);

void ini_deprecated_head(FILE* f);
void ini_deprecated_key(FILE* f, const char* key, const char* value);

int ini_deprecated_note(char* out, size_t size, const char* path, const char* const* keys, int n);

#endif
