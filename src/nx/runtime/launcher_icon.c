/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include "launcher_icon.h"
#include "launcher_ids.h"

void launcher_branding_init(HWND window) {
    RECT header = {0, 0, 200, 86}, bounds, client;
    MapDialogRect(window, &header);
    GetWindowRect(window, &bounds);
    GetClientRect(window, &client);
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT position;
        GetWindowRect(child, &position);
        MapWindowPoints(NULL, window, (POINT*)&position, 2);
        SetWindowPos(child, NULL, position.left, position.top + header.bottom, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SetWindowPos(window, NULL, bounds.left, bounds.top - header.bottom / 2,
        bounds.right - bounds.left, bounds.bottom - bounds.top + header.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT logo = {0, 4, 200, 70}, credit = {0, 71, 200, 83};
    MapDialogRect(window, &logo);
    MapDialogRect(window, &credit);
    CreateWindowExW(0, L"STATIC", L"Yo-kai Watch 4++ Recomp", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        (client.right - logo.right) / 2, logo.top, logo.right, logo.bottom - logo.top,
        window, (HMENU)(INT_PTR)IDC_BRAND_LOGO, GetModuleHandleW(NULL), NULL);
    HWND label = CreateWindowExW(0, L"STATIC", L"Logo: Pabl06", WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, credit.top, client.right, credit.bottom - credit.top,
        window, (HMENU)(INT_PTR)IDC_BRAND_CREDIT, GetModuleHandleW(NULL), NULL);
    SendMessageW(label, WM_SETFONT, SendMessageW(window, WM_GETFONT, 0, 0), FALSE);
}

int launcher_branding_draw(const DRAWITEMSTRUCT* item) {
    if (!item || item->CtlID != IDC_BRAND_LOGO) return 0;
    FillRect(item->hDC, &item->rcItem, GetSysColorBrush(COLOR_3DFACE));
    HBITMAP bitmap = LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_BRAND_LOGO),
        IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (!bitmap) return 1;
    BITMAP info;
    if (GetObjectW(bitmap, sizeof info, &info)) {
        int width = item->rcItem.right - item->rcItem.left;
        int height = MulDiv(width, info.bmHeight, info.bmWidth);
        int available_height = item->rcItem.bottom - item->rcItem.top;
        if (height > available_height) {
            height = available_height;
            width = MulDiv(height, info.bmWidth, info.bmHeight);
        }
        HDC source = CreateCompatibleDC(item->hDC);
        if (source) {
            HGDIOBJ previous = SelectObject(source, bitmap);
            BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(item->hDC, item->rcItem.left + (item->rcItem.right - item->rcItem.left - width) / 2,
                item->rcItem.top + (available_height - height) / 2, width, height,
                source, 0, 0, info.bmWidth, info.bmHeight, blend);
            SelectObject(source, previous);
            DeleteDC(source);
        }
    }
    DeleteObject(bitmap);
    return 1;
}

void launcher_icon_apply(HWND window, int has_dlc) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    LPCWSTR resource = MAKEINTRESOURCEW(has_dlc ? 2 : 1);
    HICON big = LoadImageW(instance, resource, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
    HICON small = LoadImageW(instance, resource, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (big) SendMessageW(window, WM_SETICON, ICON_BIG, (LPARAM)big);
    if (small) SendMessageW(window, WM_SETICON, ICON_SMALL, (LPARAM)small);
}

int launcher_icon_has_dlc(const wchar_t* base) {
    wchar_t pattern[32768];
    if (wcslen(base) + 64 >= 32768) return 0;
    swprintf(pattern, 32768, L"%lsYKW4_data\\system\\archives\\*.romfs", base);
    WIN32_FIND_DATAW entry;
    HANDLE find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) return 0;
    int found = 0;
    do {
        if (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if (wcslen(entry.cFileName) != 22 || _wcsicmp(entry.cFileName + 16, L".romfs")) continue;
        wchar_t* end;
        unsigned long long title = _wcstoui64(entry.cFileName, &end, 16);
        if (end == entry.cFileName + 16 && title > 0x010086c00af7d000ull && title < 0x010086c00af7d800ull &&
            (entry.nFileSizeHigh || entry.nFileSizeLow)) { found = 1; break; }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return found;
}
