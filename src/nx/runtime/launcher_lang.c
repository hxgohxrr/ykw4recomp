/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define LAUNCHER_LANG_IMPLEMENTATION
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "cJSON.h"
#include "launcher_lang.h"
#include "launcher_ids.h"

typedef struct { wchar_t id[64], name[128]; cJSON* json; } Language;
static Language languages[32];
static const wchar_t* bundled_ids[] = {L"es", L"en", L"ja", L"de"};
static int language_count, current_language;
typedef struct Text { wchar_t *source, *value; struct Text* next; } Text;
static Text* translated;
const wchar_t* launcher_lang_id(void) { return language_count ? languages[current_language].id : L"auto"; }

static wchar_t* wide(const char* text) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!n) return NULL;
    wchar_t* out = malloc((size_t)n * sizeof(wchar_t));
    if (out) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, n);
    return out;
}
const wchar_t* launcher_text(const wchar_t* source) {
    if (!source) return source;
    for (Text* t = translated; t; t = t->next) if (!wcscmp(t->source, source)) return t->value;
    return source;
}
static void dictionary_add(cJSON* json) {
    cJSON* strings = cJSON_GetObjectItemCaseSensitive(json, "strings");
    const cJSON* item;
    cJSON_ArrayForEach(item, strings) if (item->string && cJSON_IsString(item)) {
        Text* t = calloc(1, sizeof *t);
        if (!t) break;
        t->source = wide(item->string); t->value = wide(item->valuestring);
        if (!t->source || !t->value) { free(t->source); free(t->value); free(t); continue; }
        t->next = translated; translated = t;
    }
}
static void dictionary(void) {
    while (translated) { Text* next = translated->next; free(translated->source); free(translated->value); free(translated); translated = next; }
    for (size_t i = 0; i < sizeof bundled_ids / sizeof bundled_ids[0]; i++) {
        if (_wcsicmp(languages[current_language].id, bundled_ids[i])) continue;
        HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(400 + i), RT_RCDATA);
        HGLOBAL data = resource ? LoadResource(NULL, resource) : NULL;
        DWORD size = resource ? SizeofResource(NULL, resource) : 0;
        cJSON* json = data ? cJSON_ParseWithLength(LockResource(data), size) : NULL;
        dictionary_add(json);
        cJSON_Delete(json);
        break;
    }
    dictionary_add(languages[current_language].json);
}
int launcher_lang_init(const wchar_t* base, const wchar_t* ini) {
    wchar_t root[32768], path[32768], preferred[64];
    swprintf(root, 32768, L"%lslangs", base);
    CreateDirectoryW(root, NULL);
    for (size_t i = 0; i < sizeof bundled_ids / sizeof bundled_ids[0]; i++) {
        swprintf(path, 32768, L"%ls\\%ls.json", root, bundled_ids[i]);
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) continue;  
        HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(400 + i), RT_RCDATA);
        HGLOBAL data = resource ? LoadResource(NULL, resource) : NULL;
        DWORD size = resource ? SizeofResource(NULL, resource) : 0, written;
        int ok = data && WriteFile(file, LockResource(data), size, &written, NULL) && written == size;
        CloseHandle(file);
        if (!ok) DeleteFileW(path);
    }
    GetPrivateProfileStringW(L"Launcher", L"ui_language", L"es", preferred, 64, ini);
    swprintf(path, 32768, L"%ls\\*.json", root);
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW(path, &found);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            size_t n = wcslen(found.cFileName);
            if (n < 6 || n >= 64 || found.nFileSizeHigh || found.nFileSizeLow > 1024 * 1024 ||
                (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) continue;
            swprintf(path, 32768, L"%ls\\%ls", root, found.cFileName);
            FILE* file = _wfopen(path, L"rb");
            if (!file) continue;
            char* bytes = calloc((size_t)found.nFileSizeLow + 1, 1);
            size_t got = bytes ? fread(bytes, 1, found.nFileSizeLow, file) : 0;
            fclose(file);
            cJSON* json = bytes && got == found.nFileSizeLow ? cJSON_ParseWithLength(bytes, got) : NULL;
            free(bytes);
            cJSON* name = cJSON_GetObjectItemCaseSensitive(json, "name");
            if (!cJSON_IsString(name) || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, "strings"))) { cJSON_Delete(json); continue; }
            wchar_t* label = wide(name->valuestring);
            if (!label) { cJSON_Delete(json); continue; }
            Language* lang = &languages[language_count];
            wmemcpy(lang->id, found.cFileName, n - 5); lang->id[n - 5] = 0;
            wcsncpy(lang->name, label, 127); free(label);
            lang->json = json;
            if (!_wcsicmp(lang->id, preferred)) current_language = language_count;
            language_count++;
        } while (language_count < 32 && FindNextFileW(find, &found));
        FindClose(find);
    }
    if (!language_count) return 0;
    dictionary();
    return 1;
}
void launcher_lang_combo(HWND combo) {
    for (int i = 0; i < language_count; i++) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)languages[i].name);
    SendMessageW(combo, CB_SETCURSEL, current_language, 0);
}
static const wchar_t source_property[] = L"YKW4.LauncherText";
static BOOL CALLBACK apply_child(HWND child, LPARAM unused) {
    (void)unused;
    wchar_t type[64]; GetClassNameW(child, type, 64);
    if (!_wcsicmp(type, L"Edit") || !_wcsicmp(type, L"ComboBox") || !_wcsicmp(type, L"SysListView32")) return TRUE;
    wchar_t* source = GetPropW(child, source_property);
    if (!source) {
        int n = GetWindowTextLengthW(child);
        if (n <= 0 || n > 4096) return TRUE;
        source = calloc((size_t)n + 1, sizeof(wchar_t));
        if (!source) return TRUE;
        GetWindowTextW(child, source, n + 1); SetPropW(child, source_property, source);
    }
    SetWindowTextW(child, launcher_text(source));
    return TRUE;
}
void launcher_lang_apply(HWND window) {
    EnumChildWindows(window, apply_child, 0);
    const wchar_t* titles[] = {L"Juego y ajustes", L"Mods", L"Discord", L"Compilar moviles", L"Equipo"};
    for (int i = 0; i < 5; i++) {
        TCITEMW tab = {0}; tab.mask = TCIF_TEXT; tab.pszText = (wchar_t*)launcher_text(titles[i]);
        SendDlgItemMessageW(window, IDC_TABS, TCM_SETITEMW, i, (LPARAM)&tab);
    }
    RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
int launcher_lang_choose(HWND window) {
    int selected = (int)SendDlgItemMessageW(window, IDC_UI_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (selected < 0 || selected >= language_count) return 0;
    if (selected == current_language) return 0;
    current_language = selected; dictionary(); launcher_lang_apply(window); return 1;
}
int launcher_lang_save(const wchar_t* ini) {
    return WritePrivateProfileStringW(L"Launcher", L"ui_language", languages[current_language].id, ini) != 0;
}
BOOL launcher_set_control_text(HWND window, int id, const wchar_t* text) {
    HWND child = GetDlgItem(window, id);
    wchar_t* old = GetPropW(child, source_property);
    free(old);
    wchar_t* source = _wcsdup(text);
    if (source) SetPropW(child, source_property, source); else RemovePropW(child, source_property);
    return SetDlgItemTextW(window, id, launcher_text(text));
}
int launcher_message(HWND window, const wchar_t* text, const wchar_t* caption, UINT type) {
    return MessageBoxW(window, launcher_text(text), launcher_text(caption), type);
}
