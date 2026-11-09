#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <assert.h>
#include <wchar.h>
#include "launcher_ids.h"

static INT_PTR CALLBACK dialog_proc(HWND window, UINT message, WPARAM parameter, LPARAM data) {
    (void)window;
    (void)parameter;
    (void)data;
    return message == WM_INITDIALOG;
}

int main(void) {
    INITCOMMONCONTROLSEX controls = {sizeof controls, ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
    assert(InitCommonControlsEx(&controls));
    HWND window = CreateDialogParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_YKW4_LAUNCHER), NULL, dialog_proc, 0);
    assert(window);
    HWND info = GetDlgItem(window, IDC_MOBILE_INFO);
    HWND button = GetDlgItem(window, IDC_MOBILE_OPEN);
    assert(info && button);
    assert(GetDlgItem(window, IDC_BINDING_INFO) && GetDlgItem(window, IDC_BINDING_SEAL));
    wchar_t text[1024];
    assert(GetWindowTextW(info, text, 1024) > 0);
    assert(wcsstr(text, L"WSL") && wcsstr(text, L"Xcode") && wcsstr(text, L"condiciones"));
    RECT tab, bounds;
    assert(GetWindowRect(GetDlgItem(window, IDC_TABS), &tab));
    assert(GetWindowRect(button, &bounds));
    assert(bounds.left > tab.left && bounds.right < tab.right && bounds.top > tab.top && bounds.bottom < tab.bottom);
    DestroyWindow(window);
    return 0;
}
