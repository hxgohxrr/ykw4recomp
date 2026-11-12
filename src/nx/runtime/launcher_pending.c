#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "launcher_ids.h"
#include "launcher_lang.h"
#include "launcher_pending.h"

void launcher_pending_capture(HWND window, LauncherMods* mods, LauncherPending* snapshot) {
    const int edits[] = {IDC_FPS, IDC_DEADZONE, IDC_RUMBLE, IDC_RPC_APP, IDC_RPC_DETAILS,
                         IDC_RPC_CONTEXT, IDC_RPC_NORMAL, IDC_RPC_DLC};
    const int selections[] = {IDC_LANGUAGE, IDC_RESOLUTION};
    const int checks[] = {IDC_FULLSCREEN, IDC_VSYNC, IDC_SHADOWS, IDC_SWAP_AB, IDC_RPC_ENABLED, IDC_RPC_TIME};
    memset(snapshot, 0, sizeof *snapshot);
    for (unsigned index = 0; index < sizeof edits / sizeof *edits; index++)
        GetDlgItemTextW(window, edits[index], snapshot->text[index], 129);
    for (unsigned index = 0; index < sizeof selections / sizeof *selections; index++)
        snapshot->selection[index] = SendDlgItemMessageW(window, selections[index], CB_GETCURSEL, 0, 0);
    for (unsigned index = 0; index < sizeof checks / sizeof *checks; index++)
        snapshot->checks[index] = IsDlgButtonChecked(window, checks[index]);
    wcsncpy(snapshot->language, launcher_lang_id(), 15);
    launcher_mods_snapshot(window, mods, snapshot->mods);
}

int launcher_pending_confirm(HWND window, LauncherMods* mods, const LauncherPending* saved) {
    LauncherPending current;
    launcher_pending_capture(window, mods, &current);
    if (!memcmp(saved, &current, sizeof current)) return IDNO;
    return MessageBoxW(window,
        L"Hay cambios sin aplicar. ¿Quieres aplicarlos antes de jugar?\n\nSí: aplicar los cambios y jugar.\nNo: jugar con los ajustes guardados.\nCancelar: volver al launcher.",
        L"Cambios sin aplicar", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
}
