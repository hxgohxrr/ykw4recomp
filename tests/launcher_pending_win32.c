#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <wchar.h>
#include "launcher_ids.h"
#include "launcher_pending.h"

static int response = IDCANCEL, prompts;
static const wchar_t* language = L"es";
static const wchar_t* mod_state = L"first:0;second:1;";

const wchar_t* launcher_lang_id(void) { return language; }
void launcher_mods_snapshot(HWND window, LauncherMods* mods, wchar_t snapshot[8192]) {
    (void)window;
    (void)mods;
    wcscpy(snapshot, mod_state);
}
int launcher_message(HWND window, const wchar_t* text, const wchar_t* caption, UINT type) {
    assert(window && text[0] && caption[0]);
    assert((type & MB_TYPEMASK) == MB_YESNOCANCEL && (type & MB_DEFMASK) == MB_DEFBUTTON3);
    prompts++;
    return response;
}

int main(void) {
    HWND window = CreateWindowExW(0, L"STATIC", L"test", 0, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    assert(window);
    const int edits[] = {IDC_FPS, IDC_DEADZONE, IDC_RUMBLE, IDC_RPC_APP, IDC_RPC_DETAILS,
                         IDC_RPC_CONTEXT, IDC_RPC_NORMAL, IDC_RPC_DLC};
    const int checks[] = {IDC_FULLSCREEN, IDC_VSYNC, IDC_SHADOWS, IDC_SWAP_AB, IDC_RPC_ENABLED, IDC_RPC_TIME};
    const int selections[] = {IDC_LANGUAGE, IDC_RESOLUTION};
    for (unsigned index = 0; index < sizeof edits / sizeof *edits; index++)
        assert(CreateWindowExW(0, L"EDIT", L"30", WS_CHILD, 0, 0, 100, 20, window, (HMENU)(INT_PTR)edits[index], NULL, NULL));
    for (unsigned index = 0; index < sizeof checks / sizeof *checks; index++)
        assert(CreateWindowExW(0, L"BUTTON", L"test", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 100, 20, window, (HMENU)(INT_PTR)checks[index], NULL, NULL));
    for (unsigned index = 0; index < sizeof selections / sizeof *selections; index++) {
        assert(CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 100, 20, window, (HMENU)(INT_PTR)selections[index], NULL, NULL));
        SendDlgItemMessageW(window, selections[index], CB_ADDSTRING, 0, (LPARAM)L"first");
        SendDlgItemMessageW(window, selections[index], CB_ADDSTRING, 0, (LPARAM)L"second");
        SendDlgItemMessageW(window, selections[index], CB_SETCURSEL, 0, 0);
    }
    LauncherPending saved;
    launcher_pending_capture(window, NULL, &saved);
    assert(launcher_pending_confirm(window, NULL, &saved) == IDNO && !prompts);
    for (unsigned index = 0; index < sizeof edits / sizeof *edits; index++) {
        SetDlgItemTextW(window, edits[index], L"60");
        assert(launcher_pending_confirm(window, NULL, &saved) == IDCANCEL);
        SetDlgItemTextW(window, edits[index], L"30");
    }
    for (unsigned index = 0; index < sizeof checks / sizeof *checks; index++) {
        CheckDlgButton(window, checks[index], BST_CHECKED);
        assert(launcher_pending_confirm(window, NULL, &saved) == IDCANCEL);
        CheckDlgButton(window, checks[index], BST_UNCHECKED);
    }
    for (unsigned index = 0; index < sizeof selections / sizeof *selections; index++) {
        SendDlgItemMessageW(window, selections[index], CB_SETCURSEL, 1, 0);
        assert(launcher_pending_confirm(window, NULL, &saved) == IDCANCEL);
        SendDlgItemMessageW(window, selections[index], CB_SETCURSEL, 0, 0);
    }
    int previous_prompts = prompts;
    assert(launcher_pending_confirm(window, NULL, &saved) == IDNO && prompts == previous_prompts);
    language = L"en";
    assert(launcher_pending_confirm(window, NULL, &saved) == IDCANCEL);
    language = L"es";
    mod_state = L"second:1;first:0;";
    assert(launcher_pending_confirm(window, NULL, &saved) == IDCANCEL);
    mod_state = L"first:1;second:1;";
    const int choices[] = {IDYES, IDNO, IDCANCEL};
    for (unsigned index = 0; index < sizeof choices / sizeof *choices; index++) {
        response = choices[index];
        assert(launcher_pending_confirm(window, NULL, &saved) == response);
    }
    launcher_pending_capture(window, NULL, &saved);
    previous_prompts = prompts;
    assert(launcher_pending_confirm(window, NULL, &saved) == IDNO && prompts == previous_prompts);
    DestroyWindow(window);
    return 0;
}
