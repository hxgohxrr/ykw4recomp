/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <iwscapi.h>
#include <wscapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "launcher_security.h"
#include "launcher_ids.h"
#include "launcher_lang.h"

static const GUID product_list_class = {0x17072f7b,0x9abe,0x4a74,{0xa2,0x61,0x1e,0xb7,0x6b,0x55,0x10,0x7a}};
static const GUID product_list_interface = {0x722a338c,0x6e8e,0x4e72,{0xac,0x27,0x14,0x17,0xfb,0x0c,0x81,0xc2}};
static wchar_t products[1024];
static int queried, detected, avast;
static void detect(void) {
    if (queried) return;
    queried = 1;
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return;
    IWSCProductList* list = NULL;
    if (SUCCEEDED(CoCreateInstance(&product_list_class, NULL, CLSCTX_INPROC_SERVER,
                                  &product_list_interface, (void**)&list))) {
        LONG count = 0;
        if (SUCCEEDED(IWSCProductList_Initialize(list, WSC_SECURITY_PROVIDER_ANTIVIRUS)) &&
            SUCCEEDED(IWSCProductList_get_Count(list, &count))) {
            detected = 1;
            for (LONG i = 0; i < count && i < 16; ++i) {
                IWscProduct* product = NULL;
                BSTR name = NULL;
                if (SUCCEEDED(IWSCProductList_get_Item(list, (ULONG)i, &product))) {
                    if (SUCCEEDED(IWscProduct_get_ProductName(product, &name)) && name && name[0]) {
                        wchar_t lower[256];
                        size_t n = wcslen(name);
                        for (size_t j = 0; j < 255; ++j) {
                            lower[j] = j < n ? towlower(name[j]) : 0;
                            if (!lower[j]) break;
                        }
                        lower[255] = 0;
                        if (wcsstr(lower, L"avast")) avast = 1;
                        size_t used = wcslen(products);
                        if (used + n + 3 < sizeof products / sizeof *products) {
                            if (used) wcscat(products, L", ");
                            wcscat(products, name);
                        }
                    }
                    SysFreeString(name);
                    IWscProduct_Release(product);
                }
            }
        }
        IWSCProductList_Release(list);
    }
    if (SUCCEEDED(init)) CoUninitialize();
}
void launcher_security_status(HWND window, int control) {
    detect();
    wchar_t text[1200];
    if (products[0]) swprintf(text, 1200, L"%ls %ls", launcher_text(L"Antivirus registrados:"), products);
    else swprintf(text, 1200, L"%ls", launcher_text(detected ?
        L"Windows no ha informado de antivirus registrados." : L"No se pudo consultar el antivirus de Windows."));
    SetDlgItemTextW(window, control, text);
}
static int copy_folder(HWND owner, const wchar_t* folder) {
    size_t bytes = (wcslen(folder) + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!data) return 0;
    void* memory = GlobalLock(data);
    if (!memory) { GlobalFree(data); return 0; }
    memcpy(memory, folder, bytes);
    GlobalUnlock(data);
    if (!OpenClipboard(owner)) { GlobalFree(data); return 0; }
    int ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data) != NULL;
    CloseClipboard();
    if (!ok) GlobalFree(data);
    return ok;
}
static void open_target(HWND window, const wchar_t* target) {
    if ((INT_PTR)ShellExecuteW(window, L"open", target, NULL, NULL, SW_SHOWNORMAL) <= 32)
        MessageBoxW(window, L"No se pudo abrir la configuración o la guía. Abre tu antivirus desde el menú Inicio.",
                    L"Antivirus", MB_OK | MB_ICONINFORMATION);
}
static INT_PTR CALLBACK security_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(window, DWLP_USER, lp);
        launcher_lang_apply(window);
        launcher_security_status(window, IDC_SECURITY_PRODUCTS);
        SetDlgItemTextW(window, IDC_SECURITY_PATH, (const wchar_t*)lp);
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        EndDialog(window, 0); return TRUE;
    }
    if (message != WM_COMMAND) return FALSE;
    switch (LOWORD(wp)) {
        case IDC_SECURITY_COPY:
            if (copy_folder(window, (const wchar_t*)GetWindowLongPtrW(window, DWLP_USER)))
                SetDlgItemTextW(window, IDC_SECURITY_FEEDBACK, L"Ruta copiada. Pégala en el selector de exclusiones de tu antivirus.");
            else SetDlgItemTextW(window, IDC_SECURITY_FEEDBACK, L"No se pudo copiar la ruta. Selecciónala y pulsa Ctrl+C.");
            return TRUE;
        case IDC_SECURITY_OPEN: open_target(window, L"windowsdefender:"); return TRUE;
        case IDC_SECURITY_GUIDE:
            open_target(window, avast ? L"https://support.avast.com/avast/en/us/home/current/solutions/antivirus-scan-exclusions" :
                L"https://support.microsoft.com/windows/virus-and-threat-protection-in-the-windows-security-app");
            return TRUE;
    }
    return FALSE;
}
void launcher_security_help(HWND owner, const wchar_t* folder) {
    detect();
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_YKW4_SECURITY), owner, security_proc, (LPARAM)folder);
}
