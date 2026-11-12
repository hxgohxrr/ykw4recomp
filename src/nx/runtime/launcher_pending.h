#ifndef YKW4_LAUNCHER_PENDING_H
#define YKW4_LAUNCHER_PENDING_H
#include <windows.h>
#include "launcher_mods.h"

typedef struct {
    wchar_t text[8][129];
    LRESULT selection[2];
    UINT checks[6];
    wchar_t language[16];
    wchar_t mods[8192];
} LauncherPending;

void launcher_pending_capture(HWND window, LauncherMods* mods, LauncherPending* snapshot);
int launcher_pending_confirm(HWND window, LauncherMods* mods, const LauncherPending* saved);
#endif
