/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/nx/runtime/launcher_mods.c"
#include <assert.h>

const wchar_t* launcher_text(const wchar_t* source) { return source; }
BOOL launcher_set_control_text(HWND window, int id, const wchar_t* text) { (void)window; (void)id; (void)text; return TRUE; }
int launcher_message(HWND window, const wchar_t* text, const wchar_t* title, UINT flags) {
    (void)window; (void)text; (void)title; (void)flags; assert(0); return 0;
}
void launcher_catalog(HWND window, LauncherMods* mods, const wchar_t* root) { (void)window; (void)mods; (void)root; assert(0); }
int launcher_zip_extract(const wchar_t* a, const wchar_t* b, wchar_t* c, size_t n) {
    (void)a; (void)b; (void)c; (void)n; assert(0); return 0;
}

int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    wchar_t base[2048], path[2048], ini[2048];
    swprintf(base, 2048, L"%ls\\%lu-%llu\\", argv[1], GetCurrentProcessId(), GetTickCount64());
    assert(CreateDirectoryW(base, NULL));
    swprintf(path, 2048, L"%lsmods", base); assert(CreateDirectoryW(path, NULL));
    swprintf(ini, 2048, L"%lssettings.ini", base);
    for (int i = 0; i < 3; i++) {
        swprintf(path, 2048, L"%lsmods\\mod%d", base, i); assert(CreateDirectoryW(path, NULL));
        swprintf(path, 2048, L"%lsmods\\mod%d\\mod.ini", base, i);
        assert(WritePrivateProfileStringW(L"Mod", L"name", L"Synthetic test", path));
        if (i < 2) assert(WritePrivateProfileStringW(L"Mod", L"rey3ds_spanish", L"1", path));
    }
    INITCOMMONCONTROLSEX common = {sizeof common, ICC_LISTVIEW_CLASSES}; assert(InitCommonControlsEx(&common));
    HWND parent = CreateWindowExW(0, L"STATIC", L"test", 0, 0, 0, 400, 300, NULL, NULL, NULL, NULL); assert(parent);
    HWND list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT, 0, 0, 300, 200,
                                parent, (HMENU)(INT_PTR)IDC_MODLIST, NULL, NULL); assert(list);
    LauncherMods* mods = launcher_mods_load(parent, base, ini); assert(mods && mods->count == 3);
    assert(launcher_mods_has_spanish(mods));
    wchar_t saved[8192], changed[8192];
    launcher_mods_snapshot(parent, mods, saved);
    int checked = ListView_GetCheckState(list, 0) != 0;
    ListView_SetCheckState(list, 0, !checked);
    launcher_mods_snapshot(parent, mods, changed);
    assert(wcscmp(saved, changed));
    ListView_SetCheckState(list, 0, checked);
    launcher_mods_snapshot(parent, mods, changed);
    assert(!wcscmp(saved, changed));
    for (int i = 0; i < 3; i++) ListView_SetCheckState(list, i, !mods->items[i].spanish);
    launcher_mods_select_spanish(parent, mods, 1);
    int spanish = 0, other = 0;
    for (int i = 0; i < 3; i++) {
        if (mods->items[i].spanish) spanish += mods->items[i].enabled;
        else other += mods->items[i].enabled;
    }
    assert(spanish == 1 && other == 1);
    assert(launcher_mods_write_ini(parent, mods, ini));
    assert(launcher_mods_publish(mods));
    launcher_mods_select_spanish(parent, mods, 0);
    for (int i = 0; i < 3; i++) assert(mods->items[i].enabled == !mods->items[i].spanish);
    assert(launcher_mods_write_ini(parent, mods, ini));
    launcher_mods_free(mods);
    mods = launcher_mods_load(parent, base, ini);
    assert(mods && launcher_mods_has_spanish(mods));
    for (int i = 0; i < 3; i++) assert(mods->items[i].enabled == !mods->items[i].spanish);
    launcher_mods_free(mods); DestroyWindow(parent);
    return 0;
}
