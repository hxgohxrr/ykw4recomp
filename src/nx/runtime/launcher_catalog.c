/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "cJSON.h"
#include "launcher_ids.h"
#include "launcher_catalog.h"
#include "launcher_mods.h"
#include "launcher_lang.h"

typedef struct { int id, translation, code; wchar_t name[256], author[128]; } RemoteMod;
typedef struct {
    HWND window;
    LauncherMods* mods;
    wchar_t root[32768], url[4096], download[32768], error[512];
    HANDLE worker;
    volatile LONG done, cancel;
    volatile LONG64 bytes;
    int operation, closing, selected, count;
    RemoteMod entries[100];
    cJSON* result;
} Catalog;
static void utf16(const char* source, wchar_t* out, int capacity) {
    if (!source || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1, out, capacity)) out[0] = 0;
}
static const char* json_string(const cJSON* object, const char* key) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) ? value->valuestring : "";
}
static int json_id(const cJSON* object, const char* key) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(value) ? value->valueint : 0;
}


static int request(Catalog* c, int download) {
    HINTERNET session = WinHttpOpen(L"YKW4Launcher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!session) return 0;
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
    wchar_t url[4096]; wcscpy(url, c->url);
    int ok = 0;
    for (int redirect = 0; redirect < 6 && !c->cancel; redirect++) {
        wchar_t host[256], path[4096], extra[4096];
        URL_COMPONENTS parts = {0}; parts.dwStructSize = sizeof parts;
        parts.lpszHostName = host; parts.dwHostNameLength = 256;
        parts.lpszUrlPath = path; parts.dwUrlPathLength = 4096;
        parts.lpszExtraInfo = extra; parts.dwExtraInfoLength = 4096;
        if (!WinHttpCrackUrl(url, 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 ||
            (_wcsicmp(host, L"gamebanana.com") && _wcsicmp(host, L"files.gamebanana.com"))) break;
        if (wcslen(path) + wcslen(extra) >= 4096) break;
        wcscat(path, extra);
        HINTERNET connection = WinHttpConnect(session, host, 443, 0);
        HINTERNET req = connection ? WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER, status = 0, length = sizeof status;
        int sent = req && WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy) &&
            WinHttpSendRequest(req, L"Accept: application/json, application/zip, application/octet-stream\r\n", (DWORD)-1, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL) &&
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &length, NULL);
        if (!sent) { if (req) WinHttpCloseHandle(req); if (connection) WinHttpCloseHandle(connection); break; }
        if (status >= 300 && status < 400) {
            length = sizeof url;
            int moved = WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, NULL, url, &length, NULL);
            WinHttpCloseHandle(req); WinHttpCloseHandle(connection);
            if (!moved) break;
            continue;
        }
        if (status != 200) {
            swprintf(c->error, 512, L"HTTP %lu · GameBanana", status);
            WinHttpCloseHandle(req); WinHttpCloseHandle(connection); break;
        }
        FILE* file = download ? _wfopen(c->download, L"wbx") : NULL;
        char* json = download ? NULL : malloc(4 * 1024 * 1024 + 1);
        size_t used = 0;
        int complete = (download ? file != NULL : json != NULL);
        unsigned char buffer[65536];
        for (;;) {
            DWORD got = 0;
            if (c->cancel || !complete || !WinHttpReadData(req, buffer, sizeof buffer, &got)) { complete = 0; break; }
            if (!got) break;
            size_t bound = download ? 2ull * 1024 * 1024 * 1024 : 4 * 1024 * 1024;
            if (got > bound - used) { complete = 0; break; }
            if (download) { if (fwrite(buffer, 1, got, file) != got) { complete = 0; break; } }
            else memcpy(json + used, buffer, got);
            used += got; InterlockedExchange64(&c->bytes, (LONG64)used);
        }
        if (file && fclose(file)) complete = 0;
        if (complete && !download) { json[used] = 0; c->result = cJSON_ParseWithLength(json, used); complete = c->result != NULL; }
        free(json);
        WinHttpCloseHandle(req); WinHttpCloseHandle(connection);
        ok = complete && used > 0;
        break;
    }
    WinHttpCloseHandle(session);
    if (!ok && download) DeleteFileW(c->download);
    return ok;
}
static DWORD WINAPI fetch_thread(void* data) {
    Catalog* c = data;
    int ok = request(c, c->operation == 3);
    if (!ok && !c->error[0]) wcscpy(c->error, L"No se ha podido conectar con GameBanana. Comprueba la conexión y vuelve a intentarlo.");
    InterlockedExchange(&c->done, ok ? 1 : -1);
    return 0;
}
static void start(Catalog* c, int operation) {
    c->operation = operation; c->done = 0; c->cancel = 0; c->bytes = 0; c->error[0] = 0;
    cJSON_Delete(c->result); c->result = NULL;
    EnableWindow(GetDlgItem(c->window, IDC_REMOTE_REFRESH), FALSE);
    EnableWindow(GetDlgItem(c->window, IDC_REMOTE_INSTALL), FALSE);
    SetDlgItemTextW(c->window, IDC_REMOTE_STATUS, operation == 3 ? L"Descargando..." : L"Consultando GameBanana...");
    c->worker = CreateThread(NULL, 0, fetch_thread, c, 0, NULL);
    if (!c->worker) {
        InterlockedExchange(&c->done, -1);
        EnableWindow(GetDlgItem(c->window, IDC_REMOTE_REFRESH), TRUE);
        EnableWindow(GetDlgItem(c->window, IDC_REMOTE_INSTALL), TRUE);
        SetDlgItemTextW(c->window, IDC_REMOTE_STATUS, L"No se ha podido conectar con GameBanana. Comprueba la conexión y vuelve a intentarlo.");
    }
}
static void load_records(Catalog* c) {
    HWND list = GetDlgItem(c->window, IDC_REMOTE_LIST);
    ListView_DeleteAllItems(list); c->count = 0;
    const cJSON* records = cJSON_GetObjectItemCaseSensitive(c->result, "_aRecords");
    const cJSON* row;
    cJSON_ArrayForEach(row, records) {
        if (c->count == 100) break;
        const cJSON* game = cJSON_GetObjectItemCaseSensitive(row, "_aGame");
        if (json_id(game, "_idRow") != 16848) continue;
        RemoteMod* m = &c->entries[c->count];
        m->id = json_id(row, "_idRow");
        utf16(json_string(row, "_sName"), m->name, 256);
        utf16(json_string(cJSON_GetObjectItemCaseSensitive(row, "_aSubmitter"), "_sName"), m->author, 128);
        const char* category = json_string(cJSON_GetObjectItemCaseSensitive(row, "_aRootCategory"), "_sName");
        m->translation = !strcmp(category, "Translation"); m->code = !strcmp(category, "Code Patches");
        LVITEMW item = {0}; item.mask = LVIF_TEXT; item.iItem = c->count; item.pszText = m->name;
        ListView_InsertItem(list, &item);
        ListView_SetItemText(list, c->count, 1, m->author);
        ListView_SetItemText(list, c->count, 2, (wchar_t*)launcher_text(m->code ? L"Parche de código" : m->translation ? L"Traducción" : L"Mod"));
        c->count++;
    }
    SetDlgItemTextW(c->window, IDC_REMOTE_STATUS, c->count ? L"Catálogo de YKW4++. Selecciona un mod para consultar sus archivos ZIP." : L"GameBanana no ha devuelto mods para este juego.");
}
static void choose_file(Catalog* c) {
    const cJSON* game = cJSON_GetObjectItemCaseSensitive(c->result, "_aGame");
    if (json_id(game, "_idRow") != 16848 || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c->result, "_bIsWithheld")) ||
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c->result, "_bIsTrashed"))) {
        SetDlgItemTextW(c->window, IDC_REMOTE_STATUS, L"Este contenido no está disponible para instalar."); return;
    }
    TASKDIALOG_BUTTON buttons[32]; wchar_t labels[32][512], urls[32][4096]; int count = 0;
    const cJSON* row;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(c->result, "_aFiles")) {
        wchar_t name[256]; utf16(json_string(row, "_sFile"), name, 256);
        size_t n = wcslen(name);
        if (count == 32 || n < 4 || _wcsicmp(name + n - 4, L".zip") || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row, "_bIsArchived"))) continue;
        utf16(json_string(row, "_sDownloadUrl"), urls[count], 4096);
        double size = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(row, "_nFilesize"));
        if (size <= 0 || size > 2.0 * 1024 * 1024 * 1024) continue;
        swprintf(labels[count], 512, L"%ls\n%.1f MB", name, size / (1024 * 1024));
        buttons[count].nButtonID = 100 + count; buttons[count].pszButtonText = labels[count]; count++;
    }
    if (!count) { SetDlgItemTextW(c->window, IDC_REMOTE_STATUS, L"No hay archivos ZIP compatibles. Abre la página del autor para ver otros formatos o instrucciones."); return; }
    TASKDIALOGCONFIG dialog = {0}; dialog.cbSize = sizeof dialog; dialog.hwndParent = c->window;
    dialog.pszWindowTitle = L"GameBanana"; dialog.pszMainInstruction = launcher_text(L"Selecciona el archivo que quieres instalar");
    dialog.pszContent = c->entries[c->selected].name;
    dialog.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON; dialog.cButtons = count; dialog.pButtons = buttons;
    int chosen = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&dialog, &chosen, NULL, NULL)) || chosen < 100 || chosen >= 100 + count) return;
    wcscpy(c->url, urls[chosen - 100]);
    wchar_t cache[32768]; swprintf(cache, 32768, L"%ls\\.downloads", c->root); CreateDirectoryW(cache, NULL);
    swprintf(c->download, 32768, L"%ls\\%d-%016llx.zip", cache, c->entries[c->selected].id, GetTickCount64());
    start(c, 3);
}
static INT_PTR CALLBACK catalog_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    Catalog* c = (Catalog*)GetWindowLongPtrW(window, DWLP_USER);
    if (message == WM_INITDIALOG) {
        c = (Catalog*)lp; c->window = window; SetWindowLongPtrW(window, DWLP_USER, lp);
        HWND list = GetDlgItem(window, IDC_REMOTE_LIST);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const wchar_t* titles[] = {L"Nombre", L"Autor", L"Tipo"};
        RECT rect; GetClientRect(list, &rect);
        for (int i = 0; i < 3; i++) {
            LVCOLUMNW column = {0}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.pszText = (wchar_t*)launcher_text(titles[i]);
            column.cx = rect.right * (i == 0 ? 55 : i == 1 ? 25 : 20) / 100 - 2;
            ListView_InsertColumn(list, i, &column);
        }
        launcher_lang_apply(window);
        SetTimer(window, 2, 100, NULL);
        wcscpy(c->url, L"https://gamebanana.com/apiv11/Mod/Index?_aFilters%5BGeneric_Game%5D=16848&_nPerpage=50");
        start(c, 1); return TRUE;
    }
    if (!c) return FALSE;
    if (message == WM_COMMAND || message == WM_CLOSE) {
        int id = message == WM_CLOSE ? IDCANCEL : LOWORD(wp);
        if (id == IDCANCEL) {
            if (c->worker) { c->closing = 1; InterlockedExchange(&c->cancel, 1); SetDlgItemTextW(window, IDC_REMOTE_STATUS, L"Cancelando..."); }
            else EndDialog(window, 0);
            return TRUE;
        }
        if (id == IDC_REMOTE_REFRESH && !c->worker) {
            wcscpy(c->url, L"https://gamebanana.com/apiv11/Mod/Index?_aFilters%5BGeneric_Game%5D=16848&_nPerpage=50"); start(c, 1); return TRUE;
        }
        int selected = ListView_GetNextItem(GetDlgItem(window, IDC_REMOTE_LIST), -1, LVNI_SELECTED);
        if (selected >= 0 && selected < c->count && id == IDC_REMOTE_OPEN) {
            wchar_t page[128]; swprintf(page, 128, L"https://gamebanana.com/mods/%d", c->entries[selected].id);
            ShellExecuteW(window, L"open", page, NULL, NULL, SW_SHOWNORMAL); return TRUE;
        }
        if (selected >= 0 && selected < c->count && id == IDC_REMOTE_INSTALL && !c->worker) {
            if (c->entries[selected].code) { SetDlgItemTextW(window, IDC_REMOTE_STATUS, L"Los parches de código ExeFS requieren recompilar el juego y no se instalan como mods de datos."); return TRUE; }
            c->selected = selected;
            swprintf(c->url, 4096, L"https://gamebanana.com/apiv11/Mod/%d/ProfilePage", c->entries[selected].id); start(c, 2); return TRUE;
        }
    } else if (message == WM_TIMER && c->worker && c->done && WaitForSingleObject(c->worker, 0) == WAIT_OBJECT_0) {
        CloseHandle(c->worker); c->worker = NULL;
        EnableWindow(GetDlgItem(window, IDC_REMOTE_REFRESH), TRUE); EnableWindow(GetDlgItem(window, IDC_REMOTE_INSTALL), TRUE);
        if (c->closing) { EndDialog(window, 0); return TRUE; }
        if (c->done < 0) SetDlgItemTextW(window, IDC_REMOTE_STATUS, c->error);
        else if (c->operation == 1) load_records(c);
        else if (c->operation == 2) choose_file(c);
        else if (c->operation == 3) {
            wchar_t page[128]; swprintf(page, 128, L"https://gamebanana.com/mods/%d", c->entries[c->selected].id);
            int installed = launcher_mods_import_zip(GetParent(window), c->mods, c->download, c->entries[c->selected].name, c->entries[c->selected].translation, page);
            DeleteFileW(c->download);
            SetDlgItemTextW(window, IDC_REMOTE_STATUS, installed ? L"Instalado. Actívalo y guarda los cambios en la pestaña Mods." : L"No se ha instalado el mod. Revisa el formato del paquete.");
        }
        return TRUE;
    }
    return FALSE;
}
void launcher_catalog(HWND parent, LauncherMods* mods, const wchar_t* root) {
    Catalog* c = calloc(1, sizeof *c);
    if (!c) return;
    c->mods = mods; wcscpy(c->root, root);
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_YKW4_CATALOG), parent, catalog_proc, (LPARAM)c);
    cJSON_Delete(c->result); free(c);
}
