/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define COBJMACROS
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "launcher_ids.h"
#include "launcher_mods.h"
#include "launcher_zip.h"
#include "launcher_catalog.h"
#include "launcher_lang.h"

typedef struct { wchar_t id[128], key[32], name[256]; int translation, enabled, spanish; } Mod;
struct LauncherMods {
    wchar_t root[32768];
    Mod items[128];
    int count;
    int busy;
    wchar_t ini[32768];
};

static int directory(const wchar_t* path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}
static int valid_id(const wchar_t* id) {
    size_t n = wcslen(id);
    return n && n < 128 && id[0] != L'.' && !wcspbrk(id, L"/\\:*?\"<>|\r\n\t");
}
static void mod_key(Mod* mod) {
    unsigned long long hash = 1469598103934665603ull;
    for (const wchar_t* p = mod->id; *p; p++) { hash ^= (unsigned)*p; hash *= 1099511628211ull; }
    swprintf(mod->key, 32, L"%016llx", hash);
}
static void sync_checks(HWND window, LauncherMods* mods) {
    HWND list = GetDlgItem(window, IDC_MODLIST);
    for (int i = 0; i < mods->count; i++) mods->items[i].enabled = ListView_GetCheckState(list, i) != 0;
}
static void rows(HWND window, LauncherMods* mods, int selected) {
    HWND list = GetDlgItem(window, IDC_MODLIST);
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    for (int i = 0; i < mods->count; i++) {
        LVITEMW item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = mods->items[i].name;
        ListView_InsertItem(list, &item);
        ListView_SetItemText(list, i, 1, (wchar_t*)launcher_text(mods->items[i].translation ? L"Traducción" : L"Mod"));
        ListView_SetCheckState(list, i, mods->items[i].enabled);
    }
    if (selected >= 0 && selected < mods->count) {
        ListView_SetItemState(list, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list, selected, FALSE);
    }
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, NULL, TRUE);
}

LauncherMods* launcher_mods_load(HWND window, const wchar_t* base, const wchar_t* ini) {
    LauncherMods* mods = calloc(1, sizeof *mods);
    if (!mods) return NULL;
    swprintf(mods->root, 32768, L"%lsmods", base);
    wcscpy(mods->ini, ini);
    CreateDirectoryW(mods->root, NULL);
    HWND list = GetDlgItem(window, IDC_MODLIST);
    ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    RECT area;
    GetClientRect(list, &area);
    LVCOLUMNW column = {0};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = (wchar_t*)launcher_text(L"Nombre");
    column.cx = (area.right * 3) / 4;
    ListView_InsertColumn(list, 0, &column);
    column.pszText = (wchar_t*)launcher_text(L"Tipo");
    column.cx = area.right - column.cx - 6;
    ListView_InsertColumn(list, 1, &column);
    wchar_t pattern[32768], meta[32768], order[8192];
    swprintf(pattern, 32768, L"%ls\\*", mods->root);
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW(pattern, &found);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!valid_id(found.cFileName) || !(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            swprintf(meta, 32768, L"%ls\\%ls\\mod.ini", mods->root, found.cFileName);
            if (GetFileAttributesW(meta) == INVALID_FILE_ATTRIBUTES) {
                wchar_t payload[32768];
                swprintf(payload, 32768, L"%ls\\%ls\\romfs", mods->root, found.cFileName);
                if (!directory(payload)) continue;
                WritePrivateProfileStringW(L"Mod", L"name", found.cFileName, meta);
            }
            if (mods->count == 128) break;
            Mod* m = &mods->items[mods->count++];
            wcscpy(m->id, found.cFileName);
            mod_key(m);
            GetPrivateProfileStringW(L"Mod", L"name", m->id, m->name, 256, meta);
            m->spanish = GetPrivateProfileIntW(L"Mod", L"rey3ds_spanish", 0, meta) != 0;
            m->translation = GetPrivateProfileIntW(L"Mod", L"translation", 0, meta) != 0;
            m->enabled = GetPrivateProfileIntW(L"Mods", m->key, GetPrivateProfileIntW(L"Mods", m->id, 0, ini), ini) != 0;
        } while (FindNextFileW(find, &found));
        FindClose(find);
    }
    GetPrivateProfileStringW(L"Mods", L"order", L"", order, 8192, ini);
    int sorted = 0;
    wchar_t* cursor = order;
    while (*cursor && sorted < mods->count) {
        wchar_t* comma = wcschr(cursor, L',');
        if (comma) *comma = 0;
        for (int i = sorted; i < mods->count; i++) if (!wcscmp(mods->items[i].key, cursor)) {
            Mod temp = mods->items[sorted]; mods->items[sorted++] = mods->items[i]; mods->items[i] = temp;
            break;
        }
        if (!comma) break;
        cursor = comma + 1;
    }
    rows(window, mods, -1);
    return mods;
}


static int find_romfs(const wchar_t* folder, wchar_t* result, int depth, int* budget) {
    if (depth > 6 || --*budget < 0 || !directory(folder)) return 0;
    const wchar_t* name = wcsrchr(folder, L'\\');
    name = name ? name + 1 : folder;
    if (!_wcsicmp(name, L"romfs")) { wcscpy(result, folder); return 1; }
    wchar_t* paths = malloc(65536 * sizeof(wchar_t));
    if (!paths) return 0;
    wchar_t* pattern = paths;
    wchar_t* child = paths + 32768;
    if (wcslen(folder) + 300 >= 32768) { free(paths); return 0; }
    swprintf(pattern, 32768, L"%ls\\*", folder);
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) { free(paths); return 0; }
    int count = 0;
    do {
        if (entry.cFileName[0] == L'.' || !(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        swprintf(child, 32768, L"%ls\\%ls", folder, entry.cFileName);
        count += find_romfs(child, result, depth + 1, budget);
        if (count > 1) break;
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    free(paths);
    return count;
}
static int validate_payload(const wchar_t* folder, int depth, unsigned* files, ULONGLONG* bytes) {
    if (depth > 64 || !directory(folder) || wcslen(folder) + 300 >= 32768) return 0;
    wchar_t* paths = malloc(65536 * sizeof(wchar_t));
    if (!paths) return 0;
    wchar_t* pattern = paths;
    wchar_t* child = paths + 32768;
    swprintf(pattern, 32768, L"%ls\\*", folder);
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) { int empty = GetLastError() == ERROR_FILE_NOT_FOUND; free(paths); return empty; }
    int ok = 1;
    do {
        if (!wcscmp(entry.cFileName, L".") || !wcscmp(entry.cFileName, L"..")) continue;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ok = 0; break; }
        swprintf(child, 32768, L"%ls\\%ls", folder, entry.cFileName);
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ok = validate_payload(child, depth + 1, files, bytes);
        else {
            (*files)++;
            *bytes += ((ULONGLONG)entry.nFileSizeHigh << 32) | entry.nFileSizeLow;
            ok = *files <= 100000 && *bytes <= 16ull * 1024 * 1024 * 1024;
        }
        if (!ok) break;
    } while (FindNextFileW(find, &entry));
    if (ok && GetLastError() != ERROR_NO_MORE_FILES) ok = 0;
    FindClose(find);
    free(paths);
    return ok;
}

static int pick_folder(HWND window, int translation, wchar_t* selected) {
    IFileOpenDialog* dialog = NULL;
    IShellItem* item = NULL;
    PWSTR path = NULL;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                               &IID_IFileOpenDialog, (void**)&dialog))) return 0;
    IFileOpenDialog_SetOptions(dialog, FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    IFileOpenDialog_SetTitle(dialog, launcher_text(translation ? L"Añadir traducción · selecciona su carpeta romfs" : L"Añadir mod · selecciona su carpeta romfs"));
    if (SUCCEEDED(IFileOpenDialog_Show(dialog, window)) && SUCCEEDED(IFileOpenDialog_GetResult(dialog, &item)) &&
        SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path)) && wcslen(path) + 400 < 32768) {
        wcscpy(selected, path); ok = 1;
    }
    if (path) CoTaskMemFree(path);
    if (item) IShellItem_Release(item);
    IFileOpenDialog_Release(dialog);
    return ok;
}

typedef struct {
    wchar_t selected[32768], payload[32768], destination[32768], from[32770], to[32770], meta[32768];
} FolderImportPaths;
static void import_folder_with_paths(HWND window, LauncherMods* mods, int translation, FolderImportPaths* paths) {
    if (mods->count == 128) {
        MessageBoxW(window, L"Se pueden registrar hasta 128 mods.", L"Mods", MB_OK | MB_ICONINFORMATION); return;
    }
    wchar_t* selected = paths->selected;
    wchar_t* payload = paths->payload;
    wchar_t* destination = paths->destination;
    wchar_t* from = paths->from;
    wchar_t* to = paths->to;
    wchar_t* meta = paths->meta;
    if (!pick_folder(window, translation, selected)) return;
    int budget = 2048;
    unsigned files = 0;
    ULONGLONG bytes = 0;
    if (find_romfs(selected, payload, 0, &budget) != 1 || !validate_payload(payload, 0, &files, &bytes) || !files) {
        MessageBoxW(window, L"Selecciona una única carpeta romfs con archivos, o la carpeta que la contiene. Extrae antes los ZIP/RAR. No se admiten enlaces ni paquetes con varias carpetas romfs.", L"Contenido no compatible", MB_OK | MB_ICONINFORMATION);
        return;
    }
    sync_checks(window, mods);
    Mod item = {0};
    swprintf(item.id, 64, L"%016llx-%08lx-%02x", GetTickCount64(), GetCurrentProcessId(), mods->count);
    const wchar_t* label = wcsrchr(selected, L'\\');
    label = label ? label + 1 : selected;
    wcsncpy(item.name, label, 255);
    item.translation = translation != 0;
    item.spanish = translation == 2;
    if (item.spanish) wcscpy(item.name, L"Español · REY3DS 7.0");
    item.enabled = 1;
    mod_key(&item);
    if (!CreateDirectoryW(mods->root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) goto failed;
    swprintf(destination, 32768, L"%ls\\%ls", mods->root, item.id);
    if (!CreateDirectoryW(destination, NULL)) goto failed;

    wcscpy(from, payload); from[wcslen(from) + 1] = 0;
    wcscpy(to, destination); to[wcslen(to) + 1] = 0;
    SHFILEOPSTRUCTW copy = {0};
    copy.hwnd = window; copy.wFunc = FO_COPY; copy.pFrom = from; copy.pTo = to;
    copy.fFlags = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    if (SHFileOperationW(&copy) || copy.fAnyOperationsAborted) {
        SetDlgItemTextW(window, IDC_MOD_INFO, L"Importación cancelada. El mod no se ha activado."); return;
    }
    const wchar_t* leaf = wcsrchr(payload, L'\\');
    if (leaf && wcscmp(leaf + 1, L"romfs")) {
        swprintf(from, 32770, L"%ls\\%ls", destination, leaf + 1);
        swprintf(to, 32770, L"%ls\\payload", destination);
        if (!MoveFileW(from, to)) goto failed;
        swprintf(from, 32770, L"%ls\\romfs", destination);
        if (!MoveFileW(to, from)) goto failed;
    }
    swprintf(meta, 32768, L"%ls\\mod.ini", destination);
    if (!WritePrivateProfileStringW(L"Mod", L"name", item.name, meta) ||
        !WritePrivateProfileStringW(L"Mod", L"translation", translation ? L"1" : L"0", meta) ||
        (item.spanish && (!WritePrivateProfileStringW(L"Mod", L"rey3ds_spanish", L"1", meta) ||
         !WritePrivateProfileStringW(L"Mod", L"source", L"https://github.com/REY3DS/YKW4_ES", meta)))) goto failed;
    mods->items[mods->count++] = item;
    rows(window, mods, mods->count - 1);
    SetDlgItemTextW(window, IDC_MOD_INFO, L"Añadido. Activa las casillas y usa Aplicar o Jugar. Arriba tiene prioridad.");
    return;
failed:
    MessageBoxW(window, L"No se ha podido copiar el mod a la carpeta mods. No se ha activado.", L"Mods", MB_OK | MB_ICONERROR);
}

static void import_folder(HWND window, LauncherMods* mods, int translation) {
    FolderImportPaths* paths = calloc(1, sizeof *paths);
    if (!paths) {
        MessageBoxW(window, L"No hay memoria suficiente para abrir la carpeta.", L"Mods", MB_OK | MB_ICONERROR);
        return;
    }
    import_folder_with_paths(window, mods, translation, paths);
    free(paths);
}

static int mod_command(HWND window, LauncherMods* mods, int id) {
    if (!mods) return 0;
    int translation_zip = 0;
    if (id == IDC_TRANSLATION_ADD) {
        TASKDIALOG_BUTTON buttons[] = {{100, launcher_text(L"Carpeta extraída")}, {101, L"ZIP"}};
        TASKDIALOGCONFIG dialog = {0}; dialog.cbSize = sizeof dialog; dialog.hwndParent = window;
        dialog.pszWindowTitle = launcher_text(L"Traducción"); dialog.pszMainInstruction = launcher_text(L"Formato de la traducción");
        dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON; dialog.dwFlags = TDF_USE_COMMAND_LINKS;
        dialog.cButtons = 2; dialog.pButtons = buttons;
        int choice = IDCANCEL;
        if (FAILED(TaskDialogIndirect(&dialog, &choice, NULL, NULL)) || (choice != 100 && choice != 101)) return 1;
        if (choice == 100) { import_folder(window, mods, 1); return 1; }
        id = IDC_MOD_ZIP; translation_zip = 1;
    }
    if (id == IDC_MOD_ADD) { import_folder(window, mods, 0); return 1; }
    if (id == IDC_MOD_CATALOG) { launcher_catalog(window, mods, mods->root); return 1; }
    if (id == IDC_MOD_ZIP) {
        IFileOpenDialog* dialog = NULL; IShellItem* item = NULL; PWSTR path = NULL;
        if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&dialog))) {
            COMDLG_FILTERSPEC filter = {L"ZIP", L"*.zip"};
            IFileOpenDialog_SetFileTypes(dialog, 1, &filter);
            IFileOpenDialog_SetOptions(dialog, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
            if (SUCCEEDED(IFileOpenDialog_Show(dialog, window)) && SUCCEEDED(IFileOpenDialog_GetResult(dialog, &item)) &&
                SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path))) {
                const wchar_t* name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
                launcher_mods_import_zip(window, mods, path, name, translation_zip, NULL);
            }
            if (path) CoTaskMemFree(path); if (item) IShellItem_Release(item); IFileOpenDialog_Release(dialog);
        }
        return 1;
    }
    if (id == IDC_MOD_UP || id == IDC_MOD_DOWN) {
        HWND list = GetDlgItem(window, IDC_MODLIST);
        int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        int target = selected + (id == IDC_MOD_UP ? -1 : 1);
        if (selected >= 0 && target >= 0 && target < mods->count) {
            sync_checks(window, mods);
            Mod temp = mods->items[selected]; mods->items[selected] = mods->items[target]; mods->items[target] = temp;
            rows(window, mods, target);
        }
        return 1;
    }
    return 0;
}
int launcher_mods_command(HWND window, LauncherMods* mods, int id) {
    if (!mods || mods->busy) return 0;
    mods->busy = 1;
    int result = mod_command(window, mods, id);
    mods->busy = 0;
    return result;
}

int launcher_mods_import_zip(HWND window, LauncherMods* mods, const wchar_t* archive, const wchar_t* name, int translation, const wchar_t* source) {
    if (!mods || mods->count == 128) return 0;
    wchar_t destination[32768], meta[32768], error[512];
    Mod item = {0};
    swprintf(item.id, 128, L"%016llx-%08lx-%02x", GetTickCount64(), GetCurrentProcessId(), mods->count);
    wcsncpy(item.name, name, 255); item.enabled = 1; item.translation = translation;
    mod_key(&item);
    swprintf(destination, 32768, L"%ls\\%ls", mods->root, item.id);
    if (!launcher_zip_extract(archive, destination, error, 512)) {
        MessageBoxW(window, error, L"Mods", MB_OK | MB_ICONINFORMATION); return 0;
    }
    swprintf(meta, 32768, L"%ls\\mod.ini", destination);
    if (!WritePrivateProfileStringW(L"Mod", L"name", item.name, meta) ||
        !WritePrivateProfileStringW(L"Mod", L"translation", translation ? L"1" : L"0", meta) ||
        (source && !WritePrivateProfileStringW(L"Mod", L"source", source, meta))) return 0;
    sync_checks(window, mods);
    mods->items[mods->count++] = item;
    rows(window, mods, mods->count - 1);
    SetDlgItemTextW(window, IDC_MOD_INFO, L"Añadido. Activa las casillas y usa Aplicar o Jugar. Arriba tiene prioridad.");
    return 1;
}
void launcher_mods_retranslate(HWND window, LauncherMods* mods) {
    if (!mods) return;
    sync_checks(window, mods);
    HWND list = GetDlgItem(window, IDC_MODLIST);
    int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    const wchar_t* titles[] = {L"Nombre", L"Tipo"};
    for (int i = 0; i < 2; i++) {
        LVCOLUMNW column = {0}; column.mask = LVCF_TEXT; column.pszText = (wchar_t*)launcher_text(titles[i]);
        ListView_SetColumn(list, i, &column);
    }
    rows(window, mods, selected);
}

void launcher_mods_rescan(HWND window, LauncherMods* mods) {
    if (!mods || mods->count == 128 || mods->busy) return;
    wchar_t pattern[32768], path[32768], meta[32768], imports[32768];
    swprintf(pattern, 32768, L"%ls\\*", mods->root);
    swprintf(imports, 32768, L"%ls\\imports.ini", mods->root);
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) return;
    mods->busy = 1;
    int added = 0;
    sync_checks(window, mods);
    do {
        if (mods->count == 128) break;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT || entry.cFileName[0] == L'.') continue;
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!valid_id(entry.cFileName)) continue;
            int known = 0;
            for (int i = 0; i < mods->count; i++) if (!_wcsicmp(entry.cFileName, mods->items[i].id)) known = 1;
            if (known) continue;
            swprintf(path, 32768, L"%ls\\%ls\\romfs", mods->root, entry.cFileName);
            if (!directory(path)) continue;
            Mod* item = &mods->items[mods->count++];
            wcscpy(item->id, entry.cFileName); mod_key(item);
            swprintf(meta, 32768, L"%ls\\%ls\\mod.ini", mods->root, item->id);
            GetPrivateProfileStringW(L"Mod", L"name", item->id, item->name, 256, meta);
            item->spanish = GetPrivateProfileIntW(L"Mod", L"rey3ds_spanish", 0, meta) != 0;
            item->translation = GetPrivateProfileIntW(L"Mod", L"translation", 0, meta) != 0;
            item->enabled = 0; added = 1;
        } else {
            size_t length = wcslen(entry.cFileName);
            if (length < 4 || _wcsicmp(entry.cFileName + length - 4, L".zip")) continue;
            Mod stamp = {0}; wcsncpy(stamp.id, entry.cFileName, 127); mod_key(&stamp);
            wchar_t key[128];
            swprintf(key, 128, L"%ls-%08lx%08lx-%08lx%08lx", stamp.key,
                entry.nFileSizeHigh, entry.nFileSizeLow, entry.ftLastWriteTime.dwHighDateTime, entry.ftLastWriteTime.dwLowDateTime);
            if (GetPrivateProfileIntW(L"Imported", key, 0, imports)) continue;
            swprintf(path, 32768, L"%ls\\%ls", mods->root, entry.cFileName);
            FILETIME now; GetSystemTimeAsFileTime(&now);
            ULARGE_INTEGER clock, modified;
            clock.HighPart = now.dwHighDateTime; clock.LowPart = now.dwLowDateTime;
            modified.HighPart = entry.ftLastWriteTime.dwHighDateTime; modified.LowPart = entry.ftLastWriteTime.dwLowDateTime;
            if (clock.QuadPart < modified.QuadPart || clock.QuadPart - modified.QuadPart < 20000000) continue;
            HANDLE probe = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (probe == INVALID_HANDLE_VALUE) continue;
            CloseHandle(probe);

            WritePrivateProfileStringW(L"Imported", key, L"1", imports);
            launcher_mods_import_zip(window, mods, path, entry.cFileName, 0, NULL);
        }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    mods->busy = 0;
    if (added) { rows(window, mods, -1); SetDlgItemTextW(window, IDC_MOD_INFO, L"Se han detectado mods nuevos. Marca sus casillas para activarlos."); }
}

void launcher_mods_snapshot(HWND window, LauncherMods* mods, wchar_t snapshot[8192]) {
    snapshot[0] = 0;
    if (!mods) return;
    sync_checks(window, mods);
    size_t used = 0;
    for (int index = 0; index < mods->count; index++)
        used += (size_t)swprintf(snapshot + used, 8192 - used, L"%ls:%d;", mods->items[index].key, mods->items[index].enabled);
}

int launcher_mods_write_ini(HWND window, LauncherMods* mods, const wchar_t* ini) {
    if (!mods) return 0;
    sync_checks(window, mods);
    wchar_t order[8192] = L"";
    for (int i = 0; i < mods->count; i++) {
        if (i) wcscat(order, L",");
        wcscat(order, mods->items[i].key);
        if (!WritePrivateProfileStringW(L"Mods", mods->items[i].key, mods->items[i].enabled ? L"1" : L"0", ini)) return 0;
    }
    return WritePrivateProfileStringW(L"Mods", L"order", order, ini) != 0;
}
int launcher_mods_publish(LauncherMods* mods) {
    wchar_t path[32768], temporary[32768];
    if (!CreateDirectoryW(mods->root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    swprintf(path, 32768, L"%ls\\active.txt", mods->root);
    swprintf(temporary, 32768, L"%ls\\active-%lu.tmp", mods->root, GetCurrentProcessId());
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    int ok = 1;
    for (int i = 0; i < mods->count; i++) if (mods->items[i].enabled) {
        char id[516];
        int n = WideCharToMultiByte(CP_UTF8, 0, mods->items[i].id, -1, id, 513, NULL, NULL);
        DWORD written;
        if (!n) { ok = 0; break; }
        id[n - 1] = '\n';
        if (!WriteFile(file, id, (DWORD)n, &written, NULL) || written != (DWORD)n) { ok = 0; break; }
    }
    ok &= FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temporary);
    return ok;
}
void launcher_mods_free(LauncherMods* mods) { free(mods); }

int launcher_mods_has_spanish(const LauncherMods* mods) {
    if (mods) for (int i = 0; i < mods->count; i++) if (mods->items[i].spanish) return 1;
    return 0;
}
void launcher_mods_select_spanish(HWND window, LauncherMods* mods, int enabled) {
    if (!mods) return;
    sync_checks(window, mods);
    int chosen = 0;
    for (int i = 0; i < mods->count; i++) if (mods->items[i].spanish) {
        mods->items[i].enabled = enabled && !chosen;
        if (enabled) chosen = 1;
    }
    rows(window, mods, -1);
}
void launcher_mods_spanish(HWND window, LauncherMods* mods) {
    if (!mods || mods->busy) return;
    mods->busy = 1;
    TASKDIALOG_BUTTON buttons[] = {
        {100, launcher_text(L"Descargar de REY3DS")},
        {101, launcher_text(L"Instalar carpeta extraída")}
    };
    TASKDIALOGCONFIG dialog = {0}; dialog.cbSize = sizeof dialog; dialog.hwndParent = window;
    dialog.pszWindowTitle = L"Español · REY3DS 7.0";
    dialog.pszMainInstruction = launcher_text(L"Traducción española y créditos");
    dialog.pszContent = launcher_text(L"Traducción española: REY3DS / YKW4_ES.\nTextos en inglés: Vitaliy K. (@Vitalkrilov).\nBase inglesa: Yo-kai Watch 4 English Translation Mod Team.\n\nRequiere la actualización 2.2.0. Descarga el RAR de Switch y extráelo antes de instalar su carpeta romfs. El parche se obtiene del proyecto original.");
    dialog.dwCommonButtons = TDCBF_CLOSE_BUTTON; dialog.dwFlags = TDF_USE_COMMAND_LINKS;
    dialog.cButtons = 2; dialog.pButtons = buttons;
    int choice = IDCANCEL;
    if (SUCCEEDED(TaskDialogIndirect(&dialog, &choice, NULL, NULL))) {
        if (choice == 100) ShellExecuteW(window, L"open", L"https://github.com/REY3DS/YKW4_ES/releases/tag/7.0", NULL, NULL, SW_SHOWNORMAL);
        if (choice == 101) import_folder(window, mods, 2);
    }
    mods->busy = 0;
}
