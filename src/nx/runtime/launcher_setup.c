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
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "nx/loader/loader.h"
#include "nx/hle/filesystem/fs.h"
#include "launcher_setup.h"
#include "launcher_security.h"
#include "launcher_ids.h"
#include "launcher_lang.h"
#include "launcher_icon.h"
#include "launcher_binding.h"

typedef struct {
    wchar_t base[32768], executable[32768], source[32768], update[32768], dlc_folder[32768];
    wchar_t work[32768], log[32768], error[1024];
    wchar_t keys[32768], title_keys[32768];
    wchar_t dlc_packages[64][32768];
    unsigned dlc_count;
    int package_mode;
    int update_only;
    int timing_patch;
    volatile int cancel;
    volatile LONG done, stage;
    volatile LONG64 bytes_done, bytes_total;
    ULONGLONG started;
    long long log_offset;
    unsigned blocks, block_total;
    HANDLE thread;
} Setup;
static int is_file(const wchar_t* path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}
static char* utf8(const wchar_t* value) {
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, NULL, 0, NULL, NULL);
    char* bytes = size ? malloc((size_t)size) : NULL;
    if (bytes) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, bytes, size, NULL, NULL);
    return bytes;
}

static int argument(wchar_t* command, size_t capacity, const wchar_t* value) {
    size_t used = wcslen(command);
    if (used + 4 >= capacity) return 0;
    if (used) command[used++] = L' ';
    command[used++] = L'"';
    while (*value) {
        size_t slashes = 0;
        while (*value == L'\\') { slashes++; value++; }
        size_t count = (*value == L'"' || !*value) ? slashes * 2 : slashes;
        if (used + count + 4 >= capacity) return 0;
        while (count--) command[used++] = L'\\';
        if (!*value) break;
        if (*value == L'"') command[used++] = L'\\';
        command[used++] = *value++;
    }
    command[used++] = L'"'; command[used] = 0;
    return 1;
}
static int run(Setup* s, const wchar_t* const* args) {
    wchar_t command[32768] = L"";
    for (int i = 0; args[i]; i++) if (!argument(command, 32768, args[i])) return 0;
    SECURITY_ATTRIBUTES security = {sizeof security, NULL, TRUE};
    HANDLE log = CreateFileW(s->log, FILE_APPEND_DATA, FILE_SHARE_READ, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, NULL);
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (log == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE || !job) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if (job) CloseHandle(job); return 0;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    STARTUPINFOEXW start = {0}; start.StartupInfo.cb = sizeof start;
    start.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    start.StartupInfo.hStdInput = input; start.StartupInfo.hStdOutput = log; start.StartupInfo.hStdError = log;
    SIZE_T bytes = 0; InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
    start.lpAttributeList = malloc(bytes);
    HANDLE inherit[] = {input, log};
    int attrs = start.lpAttributeList && InitializeProcThreadAttributeList(start.lpAttributeList, 1, 0, &bytes);
    int ready = attrs && UpdateProcThreadAttribute(start.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, NULL, NULL);
    PROCESS_INFORMATION process = {0};
    int created = ready && CreateProcessW(args[0], command, NULL, NULL, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, s->base, &start.StartupInfo, &process);
    if (attrs) DeleteProcThreadAttributeList(start.lpAttributeList);
    free(start.lpAttributeList); CloseHandle(log); CloseHandle(input);
    int ok = 0;
    if (created) {
        if (AssignProcessToJobObject(job, process.hProcess)) {
            ResumeThread(process.hThread);
            while (WaitForSingleObject(process.hProcess, 100) == WAIT_TIMEOUT) {
                if (s->cancel) { TerminateJobObject(job, ERROR_CANCELLED); break; }
            }
            DWORD exit_code = 1;
            GetExitCodeProcess(process.hProcess, &exit_code); ok = exit_code == 0 && !s->cancel;
        } else TerminateProcess(process.hProcess, ERROR_CANCELLED);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    }
    CloseHandle(job);
    return ok;
}
static DWORD CALLBACK copy_progress(LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER stream,
    LARGE_INTEGER stream_done, DWORD number, DWORD reason, HANDLE from, HANDLE to, LPVOID data) {
    (void)total; (void)done; (void)stream; (void)stream_done; (void)number; (void)reason; (void)from; (void)to;
    Setup* s = data;
    InterlockedExchange64(&s->bytes_done, done.QuadPart);
    InterlockedExchange64(&s->bytes_total, total.QuadPart);
    return s->cancel ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}
static void pack_progress(u64 done, u64 total, void* user) {
    Setup* s = user;
    InterlockedExchange64(&s->bytes_done, done);
    InterlockedExchange64(&s->bytes_total, total);
}

static int preserve_tree(Setup* s, const wchar_t* from, const wchar_t* to) {
    DWORD attributes = GetFileAttributesW(from);
    if (attributes == INVALID_FILE_ATTRIBUTES) return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    if (s->cancel || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return CopyFileExW(from, to, copy_progress, s, NULL, COPY_FILE_FAIL_IF_EXISTS);
    if (!CreateDirectoryW(to, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    wchar_t* pattern = malloc(32768 * sizeof(wchar_t));
    wchar_t* source = malloc(32768 * sizeof(wchar_t));
    wchar_t* target = malloc(32768 * sizeof(wchar_t));
    if (!pattern || !source || !target) { free(pattern); free(source); free(target); return 0; }
    swprintf(pattern, 32768, L"%ls\\*", from);
    WIN32_FIND_DATAW entry; HANDLE find = FindFirstFileW(pattern, &entry);
    int ok = find != INVALID_HANDLE_VALUE;
    if (ok) {
        do {
            if (!wcscmp(entry.cFileName, L".") || !wcscmp(entry.cFileName, L"..")) continue;
            if (wcslen(from) + wcslen(entry.cFileName) + 2 >= 32768 || wcslen(to) + wcslen(entry.cFileName) + 2 >= 32768) { ok = 0; break; }
            swprintf(source, 32768, L"%ls\\%ls", from, entry.cFileName);
            swprintf(target, 32768, L"%ls\\%ls", to, entry.cFileName);
            if (!preserve_tree(s, source, target)) { ok = 0; break; }
        } while (FindNextFileW(find, &entry));
        if (ok && GetLastError() != ERROR_NO_MORE_FILES) ok = 0;
        FindClose(find);
    }
    free(pattern); free(source); free(target); return ok;
}
static int valid_romfs_image(const wchar_t* path) {
    FILE* file = _wfopen(path, L"rb");
    u64 header[10]; int valid = 0;
    if (!file) return 0;
    if (!_fseeki64(file, 0, SEEK_END)) {
        long long size = _ftelli64(file);
        if (size >= 80 && !_fseeki64(file, 0, SEEK_SET) && fread(header, 1, 80, file) == 80 && header[0] == 80) {
            valid = header[9] <= (u64)size;
            for (int i = 1; valid && i < 9; i += 2)
                valid = header[i] <= (u64)size && header[i + 1] <= (u64)size - header[i];
        }
    }
    fclose(file); return valid;
}
static int import_extracted_dlcs(Setup* s, const wchar_t* destination) {
    typedef struct { wchar_t pattern[32768], source[32768], target[32768], pending[32768]; } Paths;
    Paths* paths = calloc(1, sizeof *paths);
    if (!paths) return 0;
    int ok = 0, imported = 0;
    DWORD attrs = GetFileAttributesW(s->dlc_folder);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) || (attrs & FILE_ATTRIBUTE_REPARSE_POINT) || wcslen(s->dlc_folder) + 300 >= 32768) goto done;
    if (!CreateDirectoryW(destination, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) goto done;
    swprintf(paths->pattern, 32768, L"%ls\\*", s->dlc_folder);
    WIN32_FIND_DATAW entry; HANDLE find = FindFirstFileW(paths->pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) goto done;
    ok = 1;
    do {
        if (s->cancel) { ok = 0; break; }
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ok = 0; break; }
        size_t length = wcslen(entry.cFileName);
        int folder = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if ((folder && length != 16) || (!folder && (length != 22 || _wcsicmp(entry.cFileName + 16, L".romfs")))) continue;
        wchar_t id[17]; memcpy(id, entry.cFileName, 16 * sizeof(wchar_t)); id[16] = 0;
        if (wcsspn(id, L"0123456789abcdefABCDEF") != 16) { ok = 0; break; }
        unsigned long long title = wcstoull(id, NULL, 16);
        if ((title & ~0xFFFull) != 0x010086c00af7d000ull) { ok = 0; break; }
        swprintf(paths->source, 32768, folder ? L"%ls\\%ls\\romfs" : L"%ls\\%ls", s->dlc_folder, entry.cFileName);
        swprintf(paths->target, 32768, L"%ls\\%016llx.romfs", destination, title);
        swprintf(paths->pending, 32768, L"%ls\\%016llx.incoming", destination, title);
        if (GetFileAttributesW(paths->pending) != INVALID_FILE_ATTRIBUTES) { ok = 0; break; }
        if (folder) {
            char* root = utf8(paths->source); char* output = utf8(paths->pending);
            ok = root && output && !fs_pack_romfs_directory(root, output, &s->cancel, pack_progress, s);
            free(root); free(output);
        } else ok = valid_romfs_image(paths->source) && CopyFileExW(paths->source, paths->pending, copy_progress, s, NULL, COPY_FILE_FAIL_IF_EXISTS);
        if (ok) ok = valid_romfs_image(paths->pending) && MoveFileExW(paths->pending, paths->target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ok) { DeleteFileW(paths->pending); break; }
        imported++;
    } while (FindNextFileW(find, &entry));
    if (ok && GetLastError() != ERROR_NO_MORE_FILES) ok = 0;
    FindClose(find); ok = ok && imported;
done:
    free(paths); return ok;
}
static int identical_files(const wchar_t* a, const wchar_t* b) {
    FILE* first = _wfopen(a, L"rb"); FILE* second = _wfopen(b, L"rb");
    unsigned char* buffers = malloc(131072);
    int same = first && second && buffers;
    while (same) {
        size_t count = fread(buffers, 1, 65536, first);
        size_t other = fread(buffers + 65536, 1, 65536, second);
        if (count != other || memcmp(buffers, buffers + 65536, count)) { same = 0; break; }
        if (count < 65536) { same = !ferror(first) && !ferror(second); break; }
    }
    if (first) fclose(first); if (second) fclose(second); free(buffers); return same;
}
static int unpack_package_helper(const wchar_t* path) {
    HMODULE module = GetModuleHandleW(NULL);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(5001), RT_RCDATA);
    DWORD size = resource ? SizeofResource(module, resource) : 0;
    HGLOBAL loaded = resource ? LoadResource(module, resource) : NULL;
    const void* bytes = loaded ? LockResource(loaded) : NULL;
    if (!size || !bytes) return 0;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    DWORD written = 0;
    int ok = WriteFile(file, bytes, size, &written, NULL) && written == size;
    CloseHandle(file);
    if (!ok) DeleteFileW(path);
    return ok;
}
static int prepare_packages(Setup* s) {
    typedef struct { wchar_t helper[32768], backend[32768], request[32768], result[32768], output[32768], path[32768]; } Paths;
    Paths* p = calloc(1, sizeof *p);
    if (!p) return 0;
    swprintf(p->helper, 32768, L"%ls\\prepare.exe", s->work);
    swprintf(p->backend, 32768, L"%lsbackend\\hactool.exe", s->base);
    if (!is_file(p->backend) || !unpack_package_helper(p->helper)) {
        wcscpy(s->error, L"No se puede iniciar la preparación integrada. Extrae el ZIP completo del launcher.");
        free(p); return 0;
    }
    swprintf(p->request, 32768, L"%ls\\selection.ini", s->work);
    swprintf(p->result, 32768, L"%ls\\prepared.ini", s->work);
    swprintf(p->output, 32768, L"%ls\\imported", s->work);
    int ok = is_file(p->helper);
    FILE* file = ok ? _wfopen(p->request, L"wb") : NULL;
    if (file) { const unsigned char bom[] = {255, 254}; ok = fwrite(bom, 1, 2, file) == 2; fclose(file); }
    else ok = 0;
    const wchar_t* fields[] = {L"base", L"update", L"keys", L"title_keys"};
    const wchar_t* values[] = {s->source, s->update, s->keys, s->title_keys};
    for (unsigned i = 0; ok && i < 4; i++) ok = WritePrivateProfileStringW(L"Selection", fields[i], values[i], p->request);
    wchar_t count[16]; swprintf(count, 16, L"%u", s->dlc_count);
    if (ok) ok = WritePrivateProfileStringW(L"Selection", L"dlc_count", count, p->request);
    for (unsigned i = 0; ok && i < s->dlc_count; i++) {
        wchar_t key[24]; swprintf(key, 24, L"dlc%u", i);
        ok = WritePrivateProfileStringW(L"Selection", key, s->dlc_packages[i], p->request);
    }
    InterlockedExchange(&s->stage, 8);
    const wchar_t* args[] = {p->helper, L"--request", p->request, L"--output", p->output,
        L"--result", p->result, L"--backend", p->backend, L"--title-id", L"010086C00AF7C000", L"--lang", launcher_lang_id(), NULL};
    if (ok) ok = run(s, args);
    DeleteFileW(p->helper);

    const wchar_t* copies[] = {L"prod.keys", L"title.keys"};
    for (unsigned i = 0; i < 2; i++) {
        swprintf(p->path, 32768, L"%ls\\nsp-private\\backend-home\\.switch\\%ls", s->work, copies[i]);
        DeleteFileW(p->path);
    }
    if (ok) {
        ok = is_file(p->result) &&
            GetPrivateProfileStringW(L"Paths", L"exefs", L"", s->source, 32768, p->result) &&
            GetPrivateProfileStringW(L"Paths", L"romfs", L"", s->update, 32768, p->result);
        GetPrivateProfileStringW(L"Paths", L"dlc", L"", s->dlc_folder, 32768, p->result);
    }
    if (!ok) wcscpy(s->error, L"No se ha podido completar la preparación del NSP / XCI. Abre el registro de esta instalación para ver el error y la ruta de los archivos conservados, si la extracción ya había terminado.");
    free(p); return ok;
}
static int prepare(Setup* s) {
    wchar_t root[32768], input[32768], exefs[32768], out[32768], data[32768], from[32768], to[32768];
    swprintf(root, 32768, L"%lssetup-work", s->base);
    if (!CreateDirectoryW(root, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    swprintf(s->work, 32768, L"%ls\\%016llx-%lu", root, GetTickCount64(), GetCurrentProcessId());
    if (!CreateDirectoryW(s->work, NULL)) return 0;
    swprintf(s->log, 32768, L"%ls\\setup.log", s->work);
    if (s->package_mode && !prepare_packages(s)) return 0;
    swprintf(out, 32768, L"%ls\\output", s->work);
    swprintf(input, 32768, L"%ls\\input", s->work);
    swprintf(exefs, 32768, L"%ls\\exefs", input);
    swprintf(data, 32768, L"%ls\\YKW4\\YKW4_data", out);
    if (!CreateDirectoryW(input, NULL) || !CreateDirectoryW(exefs, NULL)) return 0;
    InterlockedExchange(&s->stage, 2);
    const wchar_t* modules[] = {L"rtld", L"sdk", L"main.npdm"};
    for (int i = 0; i < 3; i++) {
        swprintf(from, 32768, L"%ls\\%ls", s->source, modules[i]);
        swprintf(to, 32768, L"%ls\\%ls", exefs, modules[i]);
        if (!is_file(from) || !CopyFileW(from, to, TRUE)) return 0;
    }
    swprintf(from, 32768, L"%ls\\main", s->source);
    swprintf(to, 32768, L"%ls\\main", exefs);
    s->timing_patch = launcher_prepare_main(from, to);
    if (!s->timing_patch) {
        wcscpy(s->error, L"El ejecutable extraído no es válido. Comprueba la extracción. No se ha modificado el original."); return 0;
    }
    char* path = utf8(input); SnxNpdm npdm; char error[512];
    int valid = path && !snx_load_npdm(path, &npdm, error, sizeof error) && npdm.title_id == 0x010086c00af7c000ull;
    free(path);
    if (!valid) { wcscpy(s->error, L"La carpeta ExeFS no corresponde al juego compatible."); return 0; }
    if (s->cancel) return 0;
    InterlockedExchange(&s->stage, 3);
    const wchar_t* compile[] = {s->executable, L"--repo", s->work, L"--build-dir", s->work, L"-r", input, L"YKW4", L"--outdir", out, L"--runtime", s->executable, L"--jobs", L"4", L"-O", L"3", NULL};
    int reuse = s->update_only;
    const wchar_t* unchanged[] = {L"main", L"rtld", L"sdk", L"main.npdm"};
    for (unsigned i = 0; reuse && i < sizeof unchanged / sizeof *unchanged; i++) {
        swprintf(from, 32768, L"%ls\\%ls", exefs, unchanged[i]);
        swprintf(to, 32768, L"%lsYKW4_data\\exefs\\%ls", s->base, unchanged[i]);
        reuse = identical_files(from, to);
    }
    swprintf(from, 32768, L"%lsYKW4_code.dll", s->base);
    reuse &= is_file(from);
    if (reuse) {
        if (!CreateDirectoryW(out, NULL)) return 0;
        swprintf(to, 32768, L"%ls\\YKW4", out);
        if (!CreateDirectoryW(to, NULL) || !CreateDirectoryW(data, NULL)) return 0;
        swprintf(to, 32768, L"%ls\\YKW4\\YKW4_code.dll", out);
        if (!CopyFileExW(from, to, copy_progress, s, NULL, COPY_FILE_FAIL_IF_EXISTS)) return 0;
        swprintf(to, 32768, L"%ls\\exefs", data);
        if (!preserve_tree(s, exefs, to)) return 0;
    } else if (!run(s, compile)) { wcscpy(s->error, L"No se ha podido recompilar el juego. Consulta setup-work y vuelve a intentarlo."); return 0; }
    InterlockedExchange(&s->stage, 4);
    swprintf(to, 32768, L"%ls\\romfs.bin", data);
    char* source = utf8(s->update); char* output = utf8(to);
    int packed = s->package_mode ? valid_romfs_image(s->update) && MoveFileW(s->update, to) :
        source && output && !fs_pack_romfs_directory(source, output, &s->cancel, pack_progress, s);
    free(source); free(output);
    if (!packed) { wcscpy(s->error, L"No se ha podido preparar la carpeta RomFS. Comprueba los archivos y el espacio libre."); return 0; }
    if (s->cancel) return 0;
    if (!s->update_only) {
    InterlockedExchange(&s->stage, 7);
    const wchar_t* personal[] = {L"save", L"system", L"translations", L"sdmc"};
    for (unsigned i = 0; i < sizeof personal / sizeof *personal; i++) {
        swprintf(from, 32768, L"%lsYKW4_data\\%ls", s->base, personal[i]);
        swprintf(to, 32768, L"%ls\\%ls", data, personal[i]);
        if (!preserve_tree(s, from, to)) { wcscpy(s->error, L"No se han podido conservar los datos personales. La instalación anterior sigue intacta."); return 0; }
    }
    }
    if (s->dlc_folder[0] && s->update_only) {
        swprintf(from, 32768, L"%lsYKW4_data\\system\\archives", s->base);
        swprintf(to, 32768, L"%ls\\system", data);
        if (!CreateDirectoryW(to, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
        swprintf(to, 32768, L"%ls\\system\\archives", data);
        if (!preserve_tree(s, from, to)) return 0;
    }
    if (s->dlc_folder[0]) {
        InterlockedExchange(&s->stage, 6);
        swprintf(to, 32768, L"%ls\\system", data);
        if (!CreateDirectoryW(to, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
        swprintf(to, 32768, L"%ls\\system\\archives", data);
        if (!import_extracted_dlcs(s, to)) { wcscpy(s->error, L"No se han podido importar los DLC extraídos. Usa archivos TITULO.romfs o carpetas TITULO/romfs, con el identificador de 16 cifras del DLC."); return 0; }
    }
    if (s->cancel) return 0;
    InterlockedExchange(&s->stage, 5);
    swprintf(from, 32768, L"%lsYKW4_code.dll", s->base);
    swprintf(to, 32768, L"%ls\\YKW4\\YKW4_code.dll", out);
    int binding = launcher_binding_required(from);
    if (binding < 0 || (binding && !launcher_binding_seal(to, s->error, 1024))) {
        if (!s->error[0]) wcscpy(s->error, L"El módulo instalado no es válido. No se ha publicado la preparación.");
        return 0;
    }
    if (s->update_only) {
        swprintf(from, 32768, L"%ls\\update-backup", s->work);
        swprintf(to, 32768, L"%ls\\YKW4\\YKW4_code.dll", out);
        if (!launcher_publish_update(s->base, data, to, from)) {
            wcscpy(s->error, L"No se ha podido publicar la actualización. Cierra otras instancias del juego. La copia anterior se conserva en setup-work; las partidas no se han movido."); return 0;
        }
        return 1;
    }

    wchar_t old_data[32768], old_code[32768], new_code[32768], target_data[32768], target_code[32768];
    swprintf(old_data, 32768, L"%ls\\previous-data", s->work);
    swprintf(old_code, 32768, L"%ls\\previous-code.dll", s->work);
    swprintf(new_code, 32768, L"%ls\\YKW4\\YKW4_code.dll", out);
    swprintf(target_data, 32768, L"%lsYKW4_data", s->base);
    swprintf(target_code, 32768, L"%lsYKW4_code.dll", s->base);
    int had_data = GetFileAttributesW(target_data) != INVALID_FILE_ATTRIBUTES, had_code = is_file(target_code);
    if (had_data && !MoveFileW(target_data, old_data)) return 0;
    if (had_code && !MoveFileW(target_code, old_code)) { if (had_data) MoveFileW(old_data, target_data); return 0; }
    if (!MoveFileW(data, target_data)) goto rollback;
    if (!MoveFileW(new_code, target_code)) { MoveFileW(target_data, data); goto rollback; }
    swprintf(to, 32768, L"%lsYKW4.ini", s->base);
    if (!is_file(to)) {
        FILE* ini = _wfopen(to, L"wbx");
        if (ini) {
            fputs("[Display]\ndocked=1\nlanguage=1\n[Graphics]\nforce_visible=1\nbackface_culling=0\nresolution=1\nshadows=1\nvsync=1\n[Performance]\nfps_limit=60\n[On-screen controls]\ncontrols=0\ntouch=2\n[Debug]\ndiag=0\nprofile=0\n", ini);
            fclose(ini);
        }
    }
    if (s->timing_patch == 2) WritePrivateProfileStringW(L"Performance", L"fps_limit", L"30", to);
    return 1;
rollback:
    if (had_code) MoveFileW(old_code, target_code);
    if (had_data) MoveFileW(old_data, target_data);
    return 0;
}
static DWORD WINAPI prepare_thread(void* data) {
    Setup* s = data;
    int ok = prepare(s);
    InterlockedExchange(&s->done, ok ? 1 : -1);
    return 0;
}
static void choose_folder(HWND window, int id) {
    IFileOpenDialog* dialog = NULL; IShellItem* item = NULL; PWSTR path = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&dialog))) return;
    IFileOpenDialog_SetOptions(dialog, FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_PICKFOLDERS);
    if (SUCCEEDED(IFileOpenDialog_Show(dialog, window)) && SUCCEEDED(IFileOpenDialog_GetResult(dialog, &item)) &&
        SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path))) SetDlgItemTextW(window, id, path);
    if (path) CoTaskMemFree(path); if (item) IShellItem_Release(item); IFileOpenDialog_Release(dialog);
}
static void choose_file(HWND window, int id, int keys) {
    IFileOpenDialog* dialog = NULL; IShellItem* item = NULL; PWSTR path = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&dialog))) return;
    const COMDLG_FILTERSPEC types[] = {{keys ? L"Keys" : L"Nintendo Switch NSP / XCI", keys ? L"*.keys" : L"*.nsp;*.xci"}};
    IFileOpenDialog_SetOptions(dialog, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
    IFileOpenDialog_SetFileTypes(dialog, 1, types);
    if (SUCCEEDED(IFileOpenDialog_Show(dialog, window)) && SUCCEEDED(IFileOpenDialog_GetResult(dialog, &item)) &&
        SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path))) SetDlgItemTextW(window, id, path);
    if (path) CoTaskMemFree(path); if (item) IShellItem_Release(item); IFileOpenDialog_Release(dialog);
}
static void choose_dlc_packages(HWND window, Setup* s) {
    IFileOpenDialog* dialog = NULL; IShellItemArray* items = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&dialog))) return;
    const COMDLG_FILTERSPEC types[] = {{L"Nintendo Switch NSP / XCI", L"*.nsp;*.xci"}};
    IFileOpenDialog_SetOptions(dialog, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR | FOS_ALLOWMULTISELECT);
    IFileOpenDialog_SetFileTypes(dialog, 1, types);
    if (SUCCEEDED(IFileOpenDialog_Show(dialog, window)) && SUCCEEDED(IFileOpenDialog_GetResults(dialog, &items))) {
        DWORD count = 0; IShellItemArray_GetCount(items, &count);
        if (count > 64) MessageBoxW(window, launcher_text(L"Selecciona como máximo 64 archivos DLC."), L"YKW4Recomp", MB_OK | MB_ICONINFORMATION);
        else {
            s->dlc_count = 0;
            for (DWORD i = 0; i < count; i++) {
                IShellItem* item = NULL; PWSTR path = NULL;
                if (SUCCEEDED(IShellItemArray_GetItemAt(items, i, &item)) &&
                    SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path)) && wcslen(path) < 32768)
                    wcscpy(s->dlc_packages[s->dlc_count++], path);
                if (path) CoTaskMemFree(path); if (item) IShellItem_Release(item);
            }
            wchar_t text[64]; swprintf(text, 64, L"DLC: %u", s->dlc_count);
            SetDlgItemTextW(window, IDC_SETUP_DLC, text);
        }
    }
    if (items) IShellItemArray_Release(items); IFileOpenDialog_Release(dialog);
}
static void update_ui(HWND window, Setup* s) {
    launcher_security_status(window, IDC_SETUP_SECURITY_STATUS);
    SetWindowTextW(window, launcher_text(s->update_only ? L"Actualizar YKW4Recomp" : L"Preparar YKW4Recomp"));
    launcher_set_control_text(window, IDC_SETUP_HEADING, s->package_mode ? L"Instalar desde NSP / XCI" : L"Archivos extraídos");
    launcher_set_control_text(window, IDC_SETUP_INTRO, s->package_mode ?
        L"Selecciona el NSP o XCI del juego base y tus claves locales. La actualización y los DLC son opcionales. La preparación se realiza aquí y conserva el registro." :
        L"Selecciona las carpetas ExeFS y RomFS ya extraídas. Puedes volver al modo NSP / XCI con el botón inferior.");
    launcher_set_control_text(window, IDC_SETUP_SOURCE_LABEL, s->package_mode ? L"NSP / XCI del juego base:" : L"Carpeta ExeFS (main, rtld, sdk y main.npdm):");
    launcher_set_control_text(window, IDC_SETUP_UPDATE_LABEL, s->package_mode ? L"NSP / XCI de actualización (opcional):" : L"Carpeta RomFS (contenido del juego):");
    launcher_set_control_text(window, IDC_SETUP_DLC_LABEL, s->package_mode ? L"NSP / XCI de DLC (opcional, selección múltiple):" : L"DLC extraídos (opcional, carpeta con TITULO/romfs o TITULO.romfs):");
    launcher_set_control_text(window, IDC_SETUP_PACKAGES, s->package_mode ? L"Usar carpetas" : L"Usar NSP / XCI");
    launcher_set_control_text(window, IDC_SETUP_START, s->update_only ? L"Actualizar" : L"Preparar");
    const int ids[] = {IDC_SETUP_KEYS_LABEL, IDC_SETUP_KEYS, IDC_SETUP_KEYS_PICK, IDC_SETUP_TITLE_KEYS_LABEL, IDC_SETUP_TITLE_KEYS, IDC_SETUP_TITLE_KEYS_PICK};
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) ShowWindow(GetDlgItem(window, ids[i]), s->package_mode ? SW_SHOW : SW_HIDE);
}
static INT_PTR CALLBACK setup_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    Setup* s = (Setup*)GetWindowLongPtrW(window, DWLP_USER);
    if (message == WM_INITDIALOG) {
        s = (Setup*)lp; SetWindowLongPtrW(window, DWLP_USER, lp);
        launcher_icon_apply(window, launcher_icon_has_dlc(s->base));
        launcher_branding_init(window);
        launcher_lang_combo(GetDlgItem(window, IDC_UI_LANGUAGE));
        launcher_lang_apply(window); SetTimer(window, 5, 250, NULL);
        wchar_t keys[32768], home[32768];
        if (GetEnvironmentVariableW(L"USERPROFILE", home, 32768)) {
            swprintf(keys, 32768, L"%ls\\.switch\\prod.keys", home);
            if (is_file(keys)) SetDlgItemTextW(window, IDC_SETUP_KEYS, keys);
        }
        SetWindowTextW(window, launcher_text(L"Preparar YKW4"));
        update_ui(window, s); return TRUE;
    }
    if (message == WM_DRAWITEM && launcher_branding_draw((const DRAWITEMSTRUCT*)lp)) return TRUE;
    if (!s) return FALSE;
    if (message == WM_TIMER && wp == 5) {
        EnableWindow(GetDlgItem(window, IDC_SETUP_LOG), is_file(s->log));
        if (launcher_lang_choose(window)) update_ui(window, s);
        return TRUE;
    }
    if (message == WM_COMMAND || message == WM_CLOSE) {
        int id = message == WM_CLOSE ? IDCANCEL : LOWORD(wp);
        if (id == IDCANCEL) {
            if (s->thread) { InterlockedExchange((volatile LONG*)&s->cancel, 1); SetDlgItemTextW(window, IDC_SETUP_STATUS, L"Cancelando..."); }
            else EndDialog(window, 0);
            return TRUE;
        }
        if (id == IDC_SETUP_SECURITY) { launcher_security_help(window, s->base); return TRUE; }
        if (id == IDC_SETUP_LOG) { if (is_file(s->log)) ShellExecuteW(window, L"open", s->log, NULL, s->work, SW_SHOWNORMAL); return TRUE; }
        if (s->thread) return TRUE;
        if (id == IDC_SETUP_PACKAGES) {
            s->package_mode = !s->package_mode; s->dlc_count = 0;
            const int fields[] = {IDC_SETUP_SOURCE, IDC_SETUP_UPDATE, IDC_SETUP_DLC};
            for (unsigned i = 0; i < 3; i++) SetDlgItemTextW(window, fields[i], L"");
            update_ui(window, s);
        }
        else if (id == IDC_SETUP_SOURCE_PICK) { if (s->package_mode) choose_file(window, IDC_SETUP_SOURCE, 0); else choose_folder(window, IDC_SETUP_SOURCE); }
        else if (id == IDC_SETUP_UPDATE_PICK) { if (s->package_mode) choose_file(window, IDC_SETUP_UPDATE, 0); else choose_folder(window, IDC_SETUP_UPDATE); }
        else if (id == IDC_SETUP_DLC_PICK) { if (s->package_mode) choose_dlc_packages(window, s); else choose_folder(window, IDC_SETUP_DLC); }
        else if (id == IDC_SETUP_KEYS_PICK) choose_file(window, IDC_SETUP_KEYS, 1);
        else if (id == IDC_SETUP_TITLE_KEYS_PICK) choose_file(window, IDC_SETUP_TITLE_KEYS, 1);
        else if (id == IDC_SETUP_UPDATE_CLEAR) SetDlgItemTextW(window, IDC_SETUP_UPDATE, L"");
        else if (id == IDC_SETUP_DLC_CLEAR) { s->dlc_count = 0; SetDlgItemTextW(window, IDC_SETUP_DLC, L""); }
        else if (id == IDC_SETUP_START) {
            GetDlgItemTextW(window, IDC_SETUP_SOURCE, s->source, 32768);
            GetDlgItemTextW(window, IDC_SETUP_UPDATE, s->update, 32768);
            GetDlgItemTextW(window, IDC_SETUP_DLC, s->dlc_folder, 32768);
            GetDlgItemTextW(window, IDC_SETUP_KEYS, s->keys, 32768);
            GetDlgItemTextW(window, IDC_SETUP_TITLE_KEYS, s->title_keys, 32768);
            if (s->package_mode && (!is_file(s->source) || !is_file(s->keys) ||
                (s->update[0] && !is_file(s->update)) || (s->title_keys[0] && !is_file(s->title_keys)))) {
                MessageBoxW(window, launcher_text(L"Selecciona un NSP o XCI base y tu prod.keys. Comprueba los archivos opcionales."), L"YKW4Recomp", MB_OK | MB_ICONINFORMATION); return TRUE;
            }
            if (!s->package_mode && (!s->source[0] || !s->update[0])) { MessageBoxW(window, launcher_text(L"Selecciona las carpetas ExeFS y RomFS extraídas."), L"Preparar YKW4", MB_OK | MB_ICONINFORMATION); return TRUE; }
            wchar_t installed[32768];
            swprintf(installed, 32768, L"%lsYKW4_code.dll", s->base);
            int binding = launcher_binding_required(installed);
            unsigned char identity[32];
            if (binding < 0 || (binding && !launcher_binding_identity(identity))) {
                MessageBoxW(window, L"No se pudo comprobar el módulo o el UUID SMBIOS. No se preparará una copia sin protección.", L"Vinculación local", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            SecureZeroMemory(identity, sizeof identity);
            if (binding && MessageBoxW(window, L"El módulo preparado quedará vinculado a este PC y esta instalación de Windows. No se envía el HWID a ningún servidor. Si cambias de equipo o reinstalas Windows, prepara de nuevo desde tus archivos en otra carpeta. Las partidas no se modifican. ¿Continuar?", L"Vinculación local", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return TRUE;
            s->done = 0; s->cancel = 0; s->stage = 0; s->error[0] = 0;
            s->started = GetTickCount64(); s->log_offset = 0; s->blocks = s->block_total = 0;
            s->bytes_done = s->bytes_total = 0;
            s->thread = CreateThread(NULL, 0, prepare_thread, s, 0, NULL);
            if (!s->thread) return TRUE;
            for (int control = IDC_SETUP_SOURCE; control <= IDC_SETUP_TITLE_KEYS_PICK; control++) EnableWindow(GetDlgItem(window, control), FALSE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_UPDATE_CLEAR), FALSE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_PACKAGES), FALSE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_START), FALSE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_DLC_PICK), FALSE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_DLC_CLEAR), FALSE);
            SendDlgItemMessageW(window, IDC_SETUP_PROGRESS, PBM_SETMARQUEE, TRUE, 30);
            SetTimer(window, 4, 200, NULL);
        }
        return TRUE;
    }
    if (message == WM_TIMER && s->thread) {
        const wchar_t* stages[] = {L"Preparando...", L"Comprobando carpetas extraídas...", L"Comprobando la versión...", L"Recompilando en este PC. Puede tardar varios minutos...", L"Preparando los datos del juego...", L"Terminando la instalación...", L"Instalando los DLC seleccionados...", L"Conservando partidas y contenido instalado...", L"Leyendo y preparando los NSP / XCI seleccionados..."};
        LONG stage = s->stage;
        FILE* log = _wfopen(s->log, L"rb");
        if (log) {
            _fseeki64(log, s->log_offset, SEEK_SET);
            char line[2048];

            for (int i = 0; i < 128 && fgets(line, sizeof line, log); i++) {
                if (!strchr(line, '\n') && !strchr(line, '\r') && feof(log)) break;
                s->log_offset = _ftelli64(log);
                char* block = strstr(line, "compiled block ");
                if (block) sscanf(block, "compiled block %u/%u", &s->blocks, &s->block_total);
                wchar_t wide[2048];
                if (MultiByteToWideChar(CP_UTF8, 0, line, -1, wide, 2048)) SetDlgItemTextW(window, IDC_SETUP_DETAIL, wide);
            }
            fclose(log);
        }
        LONG64 total = InterlockedCompareExchange64(&s->bytes_total, 0, 0);
        LONG64 done = InterlockedCompareExchange64(&s->bytes_done, 0, 0);
        int percent = -1;
        if (stage == 3 && s->block_total) percent = (int)(100ull * s->blocks / s->block_total);
        if ((stage == 4 || stage == 7) && total > 0) percent = (int)(100ull * done / total);
        HWND bar = GetDlgItem(window, IDC_SETUP_PROGRESS);
        LONG style = GetWindowLongW(bar, GWL_STYLE);
        SetWindowLongW(bar, GWL_STYLE, percent < 0 ? style | PBS_MARQUEE : style & ~PBS_MARQUEE);
        SendMessageW(bar, PBM_SETMARQUEE, percent < 0, 30);
        if (percent >= 0) SendMessageW(bar, PBM_SETPOS, percent, 0);
        if (!s->cancel && stage >= 0 && stage < 9) {
            wchar_t status[1024]; ULONGLONG seconds = (GetTickCount64() - s->started) / 1000;
            swprintf(status, 1024, L"%ls\r\n%llu:%02llu  ·  %ls", launcher_text(stages[stage]), seconds / 60, seconds % 60,
                     launcher_text(L"Registro: setup-work / setup.log"));
            SetDlgItemTextW(window, IDC_SETUP_STATUS, status);
            if (percent >= 0) {
                wchar_t detail[256];
                if (stage == 3) swprintf(detail, 256, L"%u / %u  ·  %d%%", s->blocks, s->block_total, percent);
                else swprintf(detail, 256, L"%.1f / %.1f MiB  ·  %d%%", done / 1048576.0, total / 1048576.0, percent);
                SetDlgItemTextW(window, IDC_SETUP_DETAIL, detail);
            }
        }
        if (s->done && WaitForSingleObject(s->thread, 0) == WAIT_OBJECT_0) {
            CloseHandle(s->thread); s->thread = NULL; KillTimer(window, 4);
            SendDlgItemMessageW(window, IDC_SETUP_PROGRESS, PBM_SETMARQUEE, FALSE, 0);
            if (s->done > 0) {
                EndDialog(window, 1); return TRUE;
            }
            if (s->cancel) { EndDialog(window, 0); return TRUE; }
            MessageBoxW(window, s->error[0] ? s->error : L"No se ha podido preparar el juego. Revisa permisos, archivos y espacio libre. Consulta setup-work.", L"Preparar YKW4", MB_OK | MB_ICONERROR);
            for (int control = IDC_SETUP_SOURCE; control <= IDC_SETUP_TITLE_KEYS_PICK; control++) EnableWindow(GetDlgItem(window, control), TRUE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_START), TRUE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_DLC_PICK), TRUE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_UPDATE_CLEAR), TRUE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_PACKAGES), TRUE);
            EnableWindow(GetDlgItem(window, IDC_SETUP_DLC_CLEAR), TRUE);
        }
        return TRUE;
    }
    return FALSE;
}
static int setup_dialog(const wchar_t* base, const wchar_t* executable, int update_only) {
    unsigned long long hash = 1469598103934665603ull;
    for (const wchar_t* p = base; *p; p++) { hash ^= (unsigned)towlower(*p); hash *= 1099511628211ull; }
    wchar_t name[96]; swprintf(name, 96, L"Local\\YKW4-Setup-%016llx", hash);
    HANDLE mutex = CreateMutexW(NULL, TRUE, name);
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        MessageBoxW(NULL, L"Ya hay una preparación del juego abierta en esta carpeta.", L"Preparar YKW4", MB_OK | MB_ICONINFORMATION); return 0;
    }
    Setup* s = calloc(1, sizeof *s);
    if (!s) { ReleaseMutex(mutex); CloseHandle(mutex); return 0; }
    wcscpy(s->base, base); wcscpy(s->executable, executable);
    s->update_only = update_only; s->package_mode = 1;
    int result = DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_YKW4_SETUP), NULL, setup_proc, (LPARAM)s) == 1;
    free(s); ReleaseMutex(mutex); CloseHandle(mutex);
    return result;
}
int launcher_setup(const wchar_t* base, const wchar_t* executable) { return setup_dialog(base, executable, 0); }
int launcher_update(const wchar_t* base, const wchar_t* executable) { return setup_dialog(base, executable, 1); }
