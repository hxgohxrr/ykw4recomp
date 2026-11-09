#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "launcher_lang.h"

int main(int argc, char** argv) {
    assert(argc == 2);
    int german = !strcmp(argv[1], "de");
    wchar_t temporary[MAX_PATH], base[MAX_PATH], directory[MAX_PATH], path[MAX_PATH], ini[MAX_PATH];
    assert(GetTempPathW(MAX_PATH, temporary));
    assert(GetTempFileNameW(temporary, L"ykw", 0, base));
    assert(DeleteFileW(base));
    assert(CreateDirectoryW(base, NULL));
    wcscat(base, L"\\");
    swprintf(directory, MAX_PATH, L"%lslangs", base);
    assert(CreateDirectoryW(directory, NULL));
    swprintf(path, MAX_PATH, L"%ls\\%ls.json", directory, german ? L"de" : L"en");
    const char custom[] = "{\"name\":\"Custom pack\",\"strings\":{\"Salir\":\"Custom exit\"}}";
    FILE* file = _wfopen(path, L"wb");
    assert(file && fwrite(custom, 1, sizeof custom - 1, file) == sizeof custom - 1);
    assert(!fclose(file));
    swprintf(ini, MAX_PATH, L"%lsYKW4.ini", base);
    assert(WritePrivateProfileStringW(L"Launcher", L"ui_language", german ? L"de" : L"en", ini));
    assert(launcher_lang_init(base, ini));
    assert(!wcscmp(launcher_lang_id(), german ? L"de" : L"en"));
    assert(!wcscmp(launcher_text(L"Salir"), L"Custom exit"));
    assert(!wcscmp(launcher_text(L"Vinculación local"), german ? L"Lokale Bindung" : L"Local binding"));
    assert(!wcscmp(launcher_text(L"%ls (actual)"), german ? L"%ls (aktuell)" : L"%ls (current)"));
    assert(!wcscmp(launcher_text(L"Unknown diagnostic"), L"Unknown diagnostic"));
    file = _wfopen(path, L"rb");
    char bytes[sizeof custom] = {0};
    assert(file && fread(bytes, 1, sizeof bytes, file) == sizeof custom - 1);
    assert(!fclose(file) && !strcmp(bytes, custom));
    const wchar_t* ids[] = {L"es", L"en", L"ja", L"de"};
    for (unsigned index = 0; index < 4; index++) {
        swprintf(path, MAX_PATH, L"%ls\\%ls.json", directory, ids[index]);
        assert(DeleteFileW(path));
    }
    assert(DeleteFileW(ini));
    assert(RemoveDirectoryW(directory));
    assert(RemoveDirectoryW(base));
    puts("Bundled fallback and custom language pack preservation: OK");
    return 0;
}
