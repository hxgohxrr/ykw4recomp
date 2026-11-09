/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <xinput.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "launcher_ids.h"
#include "launcher_mods.h"
#include "launcher_lang.h"
#include "launcher_setup.h"
#include "launcher_icon.h"
#include "launcher_binding.h"

int ykw4_select_language(int argc);
typedef DWORD(WINAPI* ReadPad)(DWORD, XINPUT_STATE*);
typedef struct {
    wchar_t ini[32768];
    int spanish_available, original_available, spanish, ready;
    ReadPad read_pad;
    WORD previous[4];
    LauncherMods* mods;
    HWND settings_controls[32];
    unsigned settings_count;
    wchar_t base[32768];
} Launcher;

static void show_tab(HWND window, Launcher* state) {
    int tab = TabCtrl_GetCurSel(GetDlgItem(window, IDC_TABS));
    int mods = tab == 1;
    ShowWindow(GetDlgItem(window, IDC_SPANISH), mods ? SW_SHOW : SW_HIDE);
    for (unsigned i = 0; i < state->settings_count; i++) ShowWindow(state->settings_controls[i], tab == 0 ? SW_SHOW : SW_HIDE);
    for (int id = IDC_MODLIST; id <= IDC_MOD_CATALOG; id++) ShowWindow(GetDlgItem(window, id), mods ? SW_SHOW : SW_HIDE);
    for (int id = IDC_RPC_FIRST; id <= IDC_RPC_LAST; id++) ShowWindow(GetDlgItem(window, id), tab == 2 ? SW_SHOW : SW_HIDE);
    for (int id = IDC_MOBILE_FIRST; id <= IDC_MOBILE_LAST; id++) ShowWindow(GetDlgItem(window, id), tab == 3 ? SW_SHOW : SW_HIDE);
    for (int id = IDC_BINDING_FIRST; id <= IDC_BINDING_LAST; id++) ShowWindow(GetDlgItem(window, id), tab == 4 ? SW_SHOW : SW_HIDE);
}

static void bind_installation(HWND window, const Launcher* state) {
    if (!state->ready) {
        MessageBoxW(window, L"Prepara el juego antes de vincularlo.", L"Vinculación local", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (MessageBoxW(window, L"El módulo solo podrá abrirse en este PC y esta instalación de Windows. No es una prueba de propiedad ni una protección invulnerable. No se envía el HWID y no se modifican partidas. Conserva tus archivos para poder preparar de nuevo. ¿Vincular ahora?", L"Vinculación local", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return;
    wchar_t path[32768], error[1024];
    swprintf(path, 32768, L"%lsYKW4_code.dll", state->base);
    int ok = launcher_binding_seal(path, error, 1024);
    MessageBoxW(window, ok ? L"Vinculación comprobada. El módulo queda protegido para este equipo." : error, L"Vinculación local", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
}

static void open_mobile_builder(HWND window, const Launcher* state) {
    wchar_t script[32768], command[32768];
    if (wcslen(state->base) + 80 >= 32768) return;
    swprintf(script, 32768, L"%lstools\\mobile_builder.py", state->base);
    if (GetFileAttributesW(script) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(window, L"Falta tools\\mobile_builder.py junto al launcher. Instala el paquete completo.", L"YKW4Recomp", MB_OK | MB_ICONERROR);
        return;
    }
    const wchar_t* interpreters[] = {L"py -3", L"python"};
    for (unsigned index = 0; index < 2; index++) {
        STARTUPINFOW startup = {0};
        PROCESS_INFORMATION process = {0};
        startup.cb = sizeof startup;
        swprintf(command, 32768, L"%ls \"%ls\" --gui", interpreters[index], script);
        if (CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, state->base, &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return;
        }
    }
    MessageBoxW(window, L"Instala Python 3.10 o posterior con Tk para abrir el compilador movil.", L"YKW4Recomp", MB_OK | MB_ICONERROR);
}

static const int rpc_edits[] = {IDC_RPC_APP, IDC_RPC_DETAILS, IDC_RPC_CONTEXT, IDC_RPC_NORMAL, IDC_RPC_DLC};
static const wchar_t* rpc_keys[] = {L"application_id", L"details", L"context", L"normal_image", L"dlc_image"};
static const wchar_t* rpc_defaults[] = {L"1561797893252317345", L"YKW4Recomp", L"", L"", L""};
static void rpc_load(HWND window, Launcher* state) {
    wchar_t path[32768], value[129]; swprintf(path, 32768, L"%lsYKW4.discord.ini", state->base);
    for (int i = 0; i < 5; i++) {
        GetPrivateProfileStringW(L"Discord", rpc_keys[i], rpc_defaults[i], value, 129, path);
        SetDlgItemTextW(window, rpc_edits[i], value);
        SendDlgItemMessageW(window, rpc_edits[i], EM_SETLIMITTEXT, i ? 128 : 20, 0);
    }
    CheckDlgButton(window, IDC_RPC_ENABLED, GetPrivateProfileIntW(L"Discord", L"enabled", 1, path) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, IDC_RPC_TIME, GetPrivateProfileIntW(L"Discord", L"show_time", 1, path) ? BST_CHECKED : BST_UNCHECKED);
}
static int rpc_save(HWND window, Launcher* state) {
    wchar_t path[32768], temporary[32768], value[129];
    swprintf(path, 32768, L"%lsYKW4.discord.ini", state->base);
    swprintf(temporary, 32768, L"%lsYKW4.discord-%lu.tmp", state->base, GetCurrentProcessId());
    FILE* file = _wfopen(temporary, L"wb");
    if (!file) return 0;
    unsigned char bom[] = {0xFF, 0xFE};
    int ok = fwrite(bom, 1, 2, file) == 2;
    if (fclose(file)) ok = 0;
    for (int i = 0; i < 5; i++) {
        GetDlgItemTextW(window, rpc_edits[i], value, 129);
        ok &= WritePrivateProfileStringW(L"Discord", rpc_keys[i], value, temporary) != 0;
    }
    ok &= WritePrivateProfileStringW(L"Discord", L"enabled", IsDlgButtonChecked(window, IDC_RPC_ENABLED) == BST_CHECKED ? L"1" : L"0", temporary) != 0;
    ok &= WritePrivateProfileStringW(L"Discord", L"show_time", IsDlgButtonChecked(window, IDC_RPC_TIME) == BST_CHECKED ? L"1" : L"0", temporary) != 0;
    WritePrivateProfileStringW(NULL, NULL, NULL, temporary);
    if (ok) ok = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temporary);
    return ok;
}

static int regular_file(const wchar_t* path) {
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void combo_add(HWND window, int id, const wchar_t* value) {
    SendDlgItemMessageW(window, id, CB_ADDSTRING, 0, (LPARAM)launcher_text(value));
}

static void translate_choices(HWND window, Launcher* state) {
    (void)state;
    LRESULT language = SendDlgItemMessageW(window, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    SendDlgItemMessageW(window, IDC_LANGUAGE, CB_RESETCONTENT, 0, 0);
    combo_add(window, IDC_LANGUAGE, L"Original (japonés)");
    combo_add(window, IDC_LANGUAGE, L"Español · REY3DS 7.0");
    SendDlgItemMessageW(window, IDC_LANGUAGE, CB_SETCURSEL, language, 0);
    LRESULT resolution = SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
    wchar_t custom[160] = L"";
    if (SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETCOUNT, 0, 0) == 6)
        SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETLBTEXT, 5, (LPARAM)custom);
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_RESETCONTENT, 0, 0);
    const wchar_t* labels[] = {L"50 %", L"75 %", L"100 % (nativa)", L"150 %", L"200 %"};
    for (int i = 0; i < 5; i++) combo_add(window, IDC_RESOLUTION, labels[i]);
    if (custom[0]) {
        wchar_t* suffix = wcschr(custom, L' ');
        if (suffix) *suffix = 0;
        wchar_t label[160];
        swprintf(label, 160, launcher_text(L"%ls (actual)"), custom);
        combo_add(window, IDC_RESOLUTION, label);
    }
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, resolution, 0);
}

static void combo_select(HWND window, int id, const wchar_t* value) {
    LRESULT index = SendDlgItemMessageW(window, id, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)value);
    if (index == CB_ERR) index = SendDlgItemMessageW(window, id, CB_ADDSTRING, 0, (LPARAM)value);
    SendDlgItemMessageW(window, id, CB_SETCURSEL, (WPARAM)index, 0);
}

static void load_controls(HWND window, Launcher* state) {
    wchar_t value[128], label[160];
    const int combos[] = {IDC_LANGUAGE, IDC_FPS, IDC_RESOLUTION};
    for (unsigned i = 0; i < sizeof combos / sizeof *combos; i++) SendDlgItemMessageW(window, combos[i], CB_RESETCONTENT, 0, 0);
    combo_add(window, IDC_LANGUAGE, L"Original (japonés)");
    combo_add(window, IDC_LANGUAGE, L"Español · REY3DS 7.0");
    SendDlgItemMessageW(window, IDC_LANGUAGE, CB_SETCURSEL, state->spanish, 0);
    const wchar_t* limits[] = {L"30", L"60", L"120", L"180", L"game", L"0"};
    for (size_t i = 0; i < sizeof limits / sizeof *limits; i++) combo_add(window, IDC_FPS, limits[i]);
    GetPrivateProfileStringW(L"Performance", L"fps_limit", L"60", value, 128, state->ini);
    combo_select(window, IDC_FPS, value);
    SendDlgItemMessageW(window, IDC_FPS, CB_LIMITTEXT, 8, 0);
    const wchar_t* scales[] = {L"0.5", L"0.75", L"1", L"1.5", L"2"};
    const wchar_t* labels[] = {L"50 %", L"75 %", L"100 % (nativa)", L"150 %", L"200 %"};
    GetPrivateProfileStringW(L"Graphics", L"resolution", L"1", value, 128, state->ini);
    double scale = wcstod(value, NULL);
    int selected = -1;
    for (int i = 0; i < 5; i++) {
        combo_add(window, IDC_RESOLUTION, labels[i]);
        if (scale == wcstod(scales[i], NULL)) selected = i;
    }
    if (selected < 0) {
        swprintf(label, 160, launcher_text(L"%ls (actual)"), value);
        combo_add(window, IDC_RESOLUTION, label);
        selected = 5;
    }
    SendDlgItemMessageW(window, IDC_RESOLUTION, CB_SETCURSEL, selected, 0);
    CheckDlgButton(window, IDC_FULLSCREEN, GetPrivateProfileIntW(L"Display", L"fullscreen", 0, state->ini) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, IDC_VSYNC, GetPrivateProfileIntW(L"Graphics", L"vsync", 1, state->ini) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, IDC_SHADOWS, GetPrivateProfileIntW(L"Graphics", L"shadows", 1, state->ini) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, IDC_SWAP_AB, GetPrivateProfileIntW(L"Controllers", L"gamepad_swap_ab", 0, state->ini) ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(window, IDC_DEADZONE, GetPrivateProfileIntW(L"Controllers", L"gamepad_deadzone", 20, state->ini), FALSE);
    SetDlgItemInt(window, IDC_RUMBLE, GetPrivateProfileIntW(L"Controllers", L"gamepad_rumble", 100, state->ini), FALSE);
    SendDlgItemMessageW(window, IDC_DEADZONE, EM_SETLIMITTEXT, 3, 0);
    SendDlgItemMessageW(window, IDC_RUMBLE, EM_SETLIMITTEXT, 3, 0);
    SetDlgItemTextW(window, IDC_STATUS, state->ready ?
        L"El límite de FPS no garantiza el rendimiento.\nLos ajustes se guardan en YKW4.ini." :
        L"Faltan archivos del juego. Este launcher necesita la instalación local de YKW4.");
    EnableWindow(GetDlgItem(window, IDOK), state->ready);
}

static int setting(const wchar_t* path, const wchar_t* section, const wchar_t* key, const wchar_t* value) {
    return WritePrivateProfileStringW(section, key, value, path) != 0;
}

static int save_controls(HWND window, Launcher* state) {
    wchar_t fps[32], *end, staging[32768], backup[32768];
    BOOL valid_deadzone, valid_rumble;
    UINT deadzone = GetDlgItemInt(window, IDC_DEADZONE, &valid_deadzone, FALSE);
    UINT rumble = GetDlgItemInt(window, IDC_RUMBLE, &valid_rumble, FALSE);
    GetDlgItemTextW(window, IDC_FPS, fps, 32);
    wchar_t app_id[129]; GetDlgItemTextW(window, IDC_RPC_APP, app_id, 129);
    if (!app_id[0] || wcsspn(app_id, L"0123456789") != wcslen(app_id) || wcslen(app_id) > 20 || !wcstoull(app_id, NULL, 10)) {
        MessageBoxW(window, launcher_text(L"Indica un ID de aplicación de Discord válido."), L"Discord", MB_OK | MB_ICONINFORMATION); return 0;
    }
    long limit = wcstol(fps, &end, 10);
    if (_wcsicmp(fps, L"game") && (!fps[0] || *end || (limit != 0 && (limit < 10 || limit > 1000)))) {
        MessageBoxW(window, L"Indica un límite entre 10 y 1000 FPS, 0 para quitar el límite o game para usar el del juego.", L"Límite de FPS", MB_OK | MB_ICONINFORMATION);
        SetFocus(GetDlgItem(window, IDC_FPS));
        return 0;
    }
    if (!valid_deadzone || !valid_rumble || deadzone > 90 || rumble > 100) {
        MessageBoxW(window, L"La zona muerta debe estar entre 0 y 90; la vibración, entre 0 y 100.", L"Ajustes del mando", MB_OK | MB_ICONINFORMATION);
        SetFocus(GetDlgItem(window, !valid_deadzone || deadzone > 90 ? IDC_DEADZONE : IDC_RUMBLE));
        return 0;
    }

    swprintf(staging, 32768, L"%ls.launcher-%lu.tmp", state->ini, GetCurrentProcessId());
    swprintf(backup, 32768, L"%ls.launcher.bak", state->ini);
    int existed = regular_file(state->ini);
    if (existed && !CopyFileW(state->ini, staging, FALSE)) goto failed;
    if (!existed) {
        HANDLE file = CreateFileW(staging, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) goto failed;
        CloseHandle(file);
    }
    int spanish = SendDlgItemMessageW(window, IDC_LANGUAGE, CB_GETCURSEL, 0, 0) == 1;
    if (spanish && !state->spanish_available && !launcher_mods_has_spanish(state->mods)) {
        launcher_mods_spanish(window, state->mods);
        if (!launcher_mods_has_spanish(state->mods)) { DeleteFileW(staging); return 0; }
    }
    launcher_mods_select_spanish(window, state->mods, spanish);
    int ok = setting(staging, L"Launcher", L"translation", spanish ? L"es" : L"original") &&
             setting(staging, L"Performance", L"fps_limit", fps);
    int resolution = (int)SendDlgItemMessageW(window, IDC_RESOLUTION, CB_GETCURSEL, 0, 0);
    const wchar_t* scales[] = {L"0.5", L"0.75", L"1", L"1.5", L"2"};
    if (resolution >= 0 && resolution < 5) ok &= setting(staging, L"Graphics", L"resolution", scales[resolution]);
    const int ids[] = {IDC_FULLSCREEN, IDC_VSYNC, IDC_SHADOWS, IDC_SWAP_AB};
    const wchar_t* sections[] = {L"Display", L"Graphics", L"Graphics", L"Controllers"};
    const wchar_t* keys[] = {L"fullscreen", L"vsync", L"shadows", L"gamepad_swap_ab"};
    for (int i = 0; i < 4; i++) ok &= setting(staging, sections[i], keys[i], IsDlgButtonChecked(window, ids[i]) == BST_CHECKED ? L"1" : L"0");
    wchar_t number[16];
    swprintf(number, 16, L"%u", deadzone);
    ok &= setting(staging, L"Controllers", L"gamepad_deadzone", number);
    swprintf(number, 16, L"%u", rumble);
    ok &= setting(staging, L"Controllers", L"gamepad_rumble", number);
    ok &= launcher_mods_write_ini(window, state->mods, staging);
    ok &= launcher_lang_save(staging);

    WritePrivateProfileStringW(NULL, NULL, NULL, staging);
    if (!ok) goto failed;
    if (existed ? !ReplaceFileW(state->ini, staging, backup, 0, NULL, NULL) :
                  !MoveFileExW(staging, state->ini, MOVEFILE_WRITE_THROUGH)) goto failed;
    WritePrivateProfileStringW(NULL, NULL, NULL, state->ini);
    if (!launcher_mods_publish(state->mods)) {
        MessageBoxW(window, L"Se han guardado los ajustes, pero no la lista de mods. Comprueba la carpeta mods y pulsa Aplicar otra vez antes de jugar.", L"Mods", MB_OK | MB_ICONERROR);
        return 0;
    }
    state->spanish = spanish;
    if (!rpc_save(window, state)) goto failed;
    SetDlgItemTextW(window, IDC_STATUS, L"Ajustes guardados. Se aplicarán al iniciar el juego.");
    return 1;
failed:
    DeleteFileW(staging);
    MessageBoxW(window, L"No se han podido guardar los ajustes. Comprueba que la carpeta permite escritura y vuelve a intentarlo.", L"YKW4", MB_OK | MB_ICONERROR);
    return 0;
}

static INT_PTR CALLBACK launcher_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    Launcher* state = (Launcher*)GetWindowLongPtrW(window, DWLP_USER);
    if (message == WM_INITDIALOG) {
        state = (Launcher*)lp;
        SetWindowLongPtrW(window, DWLP_USER, lp);
        launcher_icon_apply(window, launcher_icon_has_dlc(state->base));
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            int id = GetDlgCtrlID(child);
            if (id == IDC_SPANISH || id == IDC_TABS || id == IDOK || id == IDCANCEL || id == IDC_APPLY || id == IDC_STATUS ||
                (id >= IDC_RPC_FIRST && id <= IDC_RPC_LAST) ||
                (id >= IDC_MOBILE_FIRST && id <= IDC_MOBILE_LAST) ||
                (id >= IDC_BINDING_FIRST && id <= IDC_BINDING_LAST) ||
                (id >= IDC_MODLIST && id <= IDC_MOD_CATALOG)) continue;
            if (state->settings_count < 32) state->settings_controls[state->settings_count++] = child;
        }
        TCITEMW tab = {0};
        launcher_branding_init(window);
        tab.mask = TCIF_TEXT;
        tab.pszText = L"Juego y ajustes";
        SendDlgItemMessageW(window, IDC_TABS, TCM_INSERTITEMW, 0, (LPARAM)&tab);
        tab.pszText = L"Mods";
        SendDlgItemMessageW(window, IDC_TABS, TCM_INSERTITEMW, 1, (LPARAM)&tab);
        tab.pszText = L"Discord";
        SendDlgItemMessageW(window, IDC_TABS, TCM_INSERTITEMW, 2, (LPARAM)&tab);
        tab.pszText = L"Compilar moviles";
        SendDlgItemMessageW(window, IDC_TABS, TCM_INSERTITEMW, 3, (LPARAM)&tab);
        tab.pszText = L"Equipo";
        SendDlgItemMessageW(window, IDC_TABS, TCM_INSERTITEMW, 4, (LPARAM)&tab);
        load_controls(window, state);
        rpc_load(window, state);
        state->mods = launcher_mods_load(window, state->base, state->ini);
        launcher_lang_combo(GetDlgItem(window, IDC_UI_LANGUAGE));
        launcher_lang_apply(window);
        show_tab(window, state);
        if (!state->mods) { EndDialog(window, 0); return TRUE; }
        SetTimer(window, 3, 2000, NULL);
        launcher_mods_rescan(window, state->mods);
        if (state->read_pad) {
            for (DWORD i = 0; i < 4; i++) {
                XINPUT_STATE pad;
                if (!state->read_pad(i, &pad)) state->previous[i] = pad.Gamepad.wButtons;
            }
            SetTimer(window, 1, 80, NULL);
        }
        SetFocus(GetDlgItem(window, state->ready ? IDOK : IDC_LANGUAGE));
        return FALSE;
    }
    if (message == WM_DRAWITEM && launcher_branding_draw((const DRAWITEMSTRUCT*)lp)) return TRUE;
    if (!state) return FALSE;
    if (message == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDC_MOBILE_OPEN) { open_mobile_builder(window, state); return TRUE; }
        if (id == IDC_BINDING_SEAL) { bind_installation(window, state); return TRUE; }
        if (id == IDC_SPANISH) {
            launcher_mods_spanish(window, state->mods);
            if (launcher_mods_has_spanish(state->mods)) SendDlgItemMessageW(window, IDC_LANGUAGE, CB_SETCURSEL, 1, 0);
            return TRUE;
        }
        if (id == IDC_INSTALL_UPDATE) {
            wchar_t executable[32768];
            if (save_controls(window, state) && GetModuleFileNameW(NULL, executable, 32768)) {
                EnableWindow(window, FALSE);
                int updated = launcher_update(state->base, executable);
                EnableWindow(window, TRUE); SetForegroundWindow(window);
                if (updated) load_controls(window, state);
            }
            return TRUE;
        }
        if (id == IDC_UI_LANGUAGE && HIWORD(wp) == CBN_SELCHANGE) {
            launcher_lang_choose(window);
            translate_choices(window, state);
            launcher_mods_retranslate(window, state->mods);
            return TRUE;
        }
        if (launcher_mods_command(window, state->mods, id)) return TRUE;
        if (id == IDCANCEL) { EndDialog(window, 0); return TRUE; }
        if (id == IDC_APPLY || id == IDOK) {
            if (id == IDOK && !state->ready) return TRUE;
            if (id == IDOK && SendDlgItemMessageW(window, IDC_LANGUAGE, CB_GETCURSEL, 0, 0) != 1 && !state->original_available) {
                MessageBoxW(window, L"No se encuentra el contenido original del juego. Selecciona una traducción instalada o restaura los archivos originales.", L"YKW4", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            if (save_controls(window, state) && id == IDOK) EndDialog(window, 1);
            return TRUE;
        }
    } else if (message == WM_NOTIFY) {
        const NMHDR* notice = (const NMHDR*)lp;
        if (notice->idFrom == IDC_TABS && notice->code == TCN_SELCHANGE) { show_tab(window, state); return TRUE; }
    } else if (message == WM_CLOSE) {
        EndDialog(window, 0);
        return TRUE;
    } else if (message == WM_TIMER && wp == 3) {
        if (launcher_lang_choose(window)) {
            translate_choices(window, state);
            launcher_mods_retranslate(window, state->mods);
        }
        launcher_mods_rescan(window, state->mods);
        return TRUE;
    } else if (message == WM_TIMER && wp == 1 && state->read_pad && GetForegroundWindow() == window) {
        for (DWORD i = 0; i < 4; i++) {
            XINPUT_STATE pad;
            WORD buttons = state->read_pad(i, &pad) ? 0 : pad.Gamepad.wButtons;
            WORD pressed = buttons & ~state->previous[i];
            state->previous[i] = buttons;
            if (pressed & XINPUT_GAMEPAD_B) SendMessageW(window, WM_COMMAND, IDCANCEL, 0);
            if (pressed & (XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_UP))
                SendMessageW(window, WM_NEXTDLGCTL, (pressed & XINPUT_GAMEPAD_DPAD_UP) != 0, FALSE);
            if (pressed & XINPUT_GAMEPAD_A) {
                HWND focused = GetFocus();
                wchar_t type[32];
                GetClassNameW(focused, type, 32);
                if (!_wcsicmp(type, L"Button")) SendMessageW(focused, BM_CLICK, 0, 0);
                else if (!_wcsicmp(type, L"ComboBox")) {
                    LRESULT count = SendMessageW(focused, CB_GETCOUNT, 0, 0);
                    LRESULT selected = SendMessageW(focused, CB_GETCURSEL, 0, 0);
                    if (count > 0) SendMessageW(focused, CB_SETCURSEL, (selected + 1) % count, 0);
                }
            }
        }
        return TRUE;
    }
    return FALSE;
}

typedef struct {
    wchar_t executable[32768], path[32768], legacy[32768], language[32];
    Launcher state;
} LauncherFrame;

static int run_launcher(int argc, LauncherFrame* frame) {
    wchar_t* executable = frame->executable;
    wchar_t* path = frame->path;
    wchar_t* legacy = frame->legacy;
    wchar_t* language = frame->language;
    DWORD length = GetModuleFileNameW(NULL, executable, 32768);
    if (argc != 1 || !length || length >= 32768) return 1;
    wchar_t* slash = wcsrchr(executable, L'\\');
    if (!slash || _wcsicmp(slash + 1, L"YKW4Recomp.exe")) return 1;
    slash[1] = 0;
    if (wcslen(executable) + 100 >= 32768) return 0;
    Launcher* state = &frame->state;
    wcscpy(state->base, executable);
    swprintf(state->ini, 32768, L"%lsYKW4.ini", executable);
    swprintf(path, 32768, L"%lsYKW4_code.dll", executable);
    state->ready = regular_file(path);
    swprintf(path, 32768, L"%lsYKW4_data\\exefs\\main", executable);
    state->ready &= regular_file(path);
    swprintf(path, 32768, L"%lsYKW4_data\\translations\\es\\romfs.bin", executable);
    state->spanish_available = regular_file(path);
    swprintf(path, 32768, L"%lsYKW4_data\\romfs.bin", executable);
    state->original_available = regular_file(path);
    state->ready &= state->spanish_available || state->original_available;
    swprintf(legacy, 32768, L"%lsYKW4.language.ini", executable);
    GetPrivateProfileStringW(L"Launcher", L"translation", L"", language, 32, state->ini);
    state->spanish = (language[0] ? !_wcsicmp(language, L"es") :
        GetPrivateProfileIntW(L"Language", L"Spanish", 0, legacy) != 0);
    HMODULE xinput = LoadLibraryExW(L"xinput1_4.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!xinput) xinput = LoadLibraryExW(L"xinput9_1_0.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (xinput) state->read_pad = (ReadPad)GetProcAddress(xinput, "XInputGetState");
    INITCOMMONCONTROLSEX controls = {sizeof controls, ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    if (!launcher_lang_init(executable, state->ini)) {
        MessageBoxW(NULL, L"No se han podido cargar los idiomas del launcher desde langs.", L"YKW4", MB_OK | MB_ICONERROR);
        return 0;
    }
    HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INT_PTR result = 0;
    swprintf(path, 32768, L"%lsYKW4Recomp.exe", state->base);
    if (state->ready || launcher_setup(state->base, path)) {
        swprintf(path, 32768, L"%lsYKW4_data\\romfs.bin", state->base);
        state->original_available = regular_file(path);
        state->ready = 1;
        result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_YKW4_LAUNCHER), NULL, launcher_proc, (LPARAM)state);
    }
    int legacy_spanish = state->spanish && state->spanish_available && !launcher_mods_has_spanish(state->mods);
    launcher_mods_free(state->mods);
    if (SUCCEEDED(com)) CoUninitialize();
    if (xinput) FreeLibrary(xinput);
    if (result == -1) MessageBoxW(NULL, L"No se ha podido abrir el launcher.", L"YKW4", MB_OK | MB_ICONERROR);
    return result == 1 && !_putenv_s("YKW4_LANGUAGE", legacy_spanish ? "es" : "original") ? 2 : 0;
}

int ykw4_select_language(int argc) {
    if (argc != 1) return 1;
    LauncherFrame* frame = calloc(1, sizeof *frame);
    if (!frame) return 0;
    int result = run_launcher(argc, frame);
    free(frame);
    return result;
}
