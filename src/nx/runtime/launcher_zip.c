/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <stdint.h>
#include <io.h>
#include "miniz.h"
#include "launcher_zip.h"

typedef struct { wchar_t* name; mz_uint index; int directory; } ZipEntry;
static size_t zip_read(void* file, mz_uint64 offset, void* data, size_t size) {
    return _fseeki64(file, (long long)offset, SEEK_SET) ? 0 : fread(data, 1, size, file);
}
static size_t zip_write(void* file, mz_uint64 offset, const void* data, size_t size) {
    return _fseeki64(file, (long long)offset, SEEK_SET) ? 0 : fwrite(data, 1, size, file);
}
static int entry_compare(const void* left, const void* right) {
    return _wcsicmp(((const ZipEntry*)left)->name, ((const ZipEntry*)right)->name);
}
static int safe_name(wchar_t* name) {
    if (!*name || name[0] == L'/' || name[0] == L'\\') return 0;
    for (wchar_t* p = name; *p; p++) {
        if (*p == L'\\') *p = L'/';
        if (*p < 32 || wcschr(L":*?\"<>|", *p)) return 0;
    }
    wchar_t* part = name;
    for (;;) {
        wchar_t* slash = wcschr(part, L'/');
        size_t n = slash ? (size_t)(slash - part) : wcslen(part);
        if (!n || n > 255 || part[n - 1] == L'.' || part[n - 1] == L' ') return 0;
        wchar_t device[256]; wmemcpy(device, part, n); device[n] = 0;
        wchar_t* dot = wcschr(device, L'.'); if (dot) *dot = 0;
        if (!_wcsicmp(device, L"CON") || !_wcsicmp(device, L"PRN") || !_wcsicmp(device, L"AUX") || !_wcsicmp(device, L"NUL") ||
            !_wcsicmp(device, L"CONIN$") || !_wcsicmp(device, L"CONOUT$") ||
            ((!_wcsnicmp(device, L"COM", 3) || !_wcsnicmp(device, L"LPT", 3)) &&
             wcschr(L"\u00b9\u00b2\u00b3", device[3]) && device[3] && !device[4]) ||
            (!_wcsnicmp(device, L"COM", 3) && device[3] >= L'0' && device[3] <= L'9' && !device[4]) ||
            (!_wcsnicmp(device, L"LPT", 3) && device[3] >= L'0' && device[3] <= L'9' && !device[4])) return 0;
        if (!slash || !slash[1]) return 1;
        part = slash + 1;
    }
}
static int make_parents(wchar_t* path, size_t root_length) {
    for (wchar_t* p = path + root_length + 1; *p; p++) if (*p == L'\\') {
        *p = 0;
        if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) { *p = L'\\'; return 0; }
        DWORD a = GetFileAttributesW(path);
        *p = L'\\';
        if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY) || (a & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
    }
    return 1;
}
int launcher_zip_extract(const wchar_t* archive, const wchar_t* destination, wchar_t* error, size_t capacity) {
    FILE* input = _wfopen(archive, L"rb");
    mz_zip_archive zip = {0};
    ZipEntry* entries = NULL;
    unsigned count = 0;
    int initialized = 0, ok = 0;
    const wchar_t* reason = L"No se ha podido abrir el ZIP.";
    if (!input) goto done;
    if (_fseeki64(input, 0, SEEK_END)) goto done;
    long long size = _ftelli64(input);
    if (size < 22 || size > 4ll * 1024 * 1024 * 1024) goto done;
    zip.m_pRead = zip_read; zip.m_pIO_opaque = input;
    if (!mz_zip_reader_init(&zip, (mz_uint64)size, 0)) goto done;
    initialized = 1;
    count = mz_zip_reader_get_num_files(&zip);
    if (!count || count > 100000) goto done;
    entries = calloc(count, sizeof *entries);
    if (!entries) goto done;
    mz_uint64 total = 0;
    reason = L"El ZIP contiene rutas inseguras, archivos duplicados o contenido no compatible.";
    wchar_t prefix[2048] = L"";
    int has_prefix = 0;
    for (unsigned i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_encrypted ||
            stat.m_uncomp_size > 2ull * 1024 * 1024 * 1024 ||
            (stat.m_method != 0 && stat.m_method != 8)) goto done;
        unsigned mode = (stat.m_external_attr >> 16) & 0170000;
        if (mode && mode != 0100000 && mode != 0040000) goto done;
        total += stat.m_uncomp_size;
        if (total > 16ull * 1024 * 1024 * 1024) goto done;
        mz_uint length = mz_zip_reader_get_filename(&zip, i, NULL, 0);
        if (!length || length > 4096) goto done;
        char* utf8 = malloc(length);
        if (!utf8) goto done;
        mz_zip_reader_get_filename(&zip, i, utf8, length);

        if (strlen(utf8) + 1 != length) { free(utf8); goto done; }
        UINT codepage = (stat.m_bit_flag & 0x800) ? CP_UTF8 : 437;
        int chars = MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, utf8, -1, NULL, 0);
        if (!chars || chars > 2048) { free(utf8); goto done; }
        entries[i].name = malloc((size_t)chars * sizeof(wchar_t));
        if (!entries[i].name) { free(utf8); goto done; }
        MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, utf8, -1, entries[i].name, chars);
        free(utf8);
        if (!safe_name(entries[i].name)) goto done;
        entries[i].index = i; entries[i].directory = stat.m_is_directory;
        wchar_t* component = entries[i].name;
        while (*component) {
            if (!_wcsnicmp(component, L"romfs/", 6)) {
                size_t n = (size_t)(component - entries[i].name) + 6;
                wchar_t candidate[2048]; wmemcpy(candidate, entries[i].name, n); candidate[n] = 0;
                if (has_prefix && _wcsicmp(prefix, candidate)) goto done;
                wcscpy(prefix, candidate); has_prefix = 1; break;
            }
            wchar_t* slash = wcschr(component, L'/');
            if (!slash) break;
            component = slash + 1;
        }
    }
    qsort(entries, count, sizeof *entries, entry_compare);
    for (unsigned i = 1; i < count; i++) if (!_wcsicmp(entries[i - 1].name, entries[i].name)) goto done;
    if (!has_prefix) {

        int data = 0;
        for (unsigned i = 0; i < count; i++) if (!_wcsnicmp(entries[i].name, L"data/", 5)) data = 1;
        if (!data) { reason = L"El ZIP no contiene una única carpeta romfs ni data. Los parches de código ExeFS no son compatibles."; goto done; }
    }
    if (!CreateDirectoryW(destination, NULL)) goto done;  
    size_t root_length = wcslen(destination), prefix_length = wcslen(prefix);
    unsigned extracted = 0;
    for (unsigned i = 0; i < count; i++) {
        ZipEntry* entry = &entries[i];
        if (entry->directory) continue;
        if (has_prefix && _wcsnicmp(entry->name, prefix, prefix_length)) continue;
        if (!has_prefix && _wcsnicmp(entry->name, L"data/", 5)) continue;
        const wchar_t* relative = entry->name + prefix_length;
        if (!*relative) continue;
        wchar_t path[32768];
        if (root_length + wcslen(relative) + 9 >= 32768) goto done;
        swprintf(path, 32768, L"%ls\\romfs\\%ls", destination, relative);
        for (wchar_t* p = path; *p; p++) if (*p == L'/') *p = L'\\';
        if (!make_parents(path, root_length)) goto done;
        HANDLE handle = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE) goto done;
        int fd = _open_osfhandle((intptr_t)handle, 0);
        if (fd < 0) { CloseHandle(handle); goto done; }
        FILE* output = _fdopen(fd, "wb");
        if (!output) { _close(fd); goto done; }
        int success = mz_zip_reader_extract_to_callback(&zip, entry->index, zip_write, output, 0);
        success &= fclose(output) == 0;
        if (!success) { reason = L"El ZIP está dañado o no se ha podido escribir su contenido."; goto done; }
        extracted++;
    }
    if (!extracted) goto done;
    ok = 1;
done:
    if (entries) { for (unsigned i = 0; i < count; i++) free(entries[i].name); free(entries); }
    if (initialized) mz_zip_reader_end(&zip);
    if (input) fclose(input);
    if (!ok && capacity) { wcsncpy(error, reason, capacity - 1); error[capacity - 1] = 0; }
    return ok;
}
