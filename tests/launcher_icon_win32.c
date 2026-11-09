/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/nx/runtime/launcher_icon.c"
#include <assert.h>
#include <stdio.h>
int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    wchar_t base[2048], path[2048];
    swprintf(base, 2048, L"%ls\\%lu-%llu\\", argv[1], GetCurrentProcessId(), GetTickCount64());
    assert(CreateDirectoryW(base, NULL));
    assert(!launcher_icon_has_dlc(base));
    const wchar_t* folders[] = {L"YKW4_data", L"YKW4_data\\system", L"YKW4_data\\system\\archives"};
    for (int i = 0; i < 3; i++) { swprintf(path,2048,L"%ls%ls",base,folders[i]); assert(CreateDirectoryW(path,NULL)); }
    swprintf(path,2048,L"%lsYKW4_data\\system\\archives\\010086c00af7c800.romfs",base);
    FILE* f=_wfopen(path,L"wb");assert(f);fputs("synthetic update",f);fclose(f);
    assert(!launcher_icon_has_dlc(base));
    swprintf(path,2048,L"%lsYKW4_data\\system\\archives\\010086c00af7d064.romfs",base);
    f=_wfopen(path,L"wb");assert(f);fclose(f);assert(!launcher_icon_has_dlc(base));
    f=_wfopen(path,L"wb");assert(f);fputs("synthetic nonempty DLC marker",f);fclose(f);
    assert(launcher_icon_has_dlc(base));
    for (int resource=1;resource<=2;resource++) {
        for (int size=16;size<=32;size*=2) {
            HICON icon=LoadImageW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(resource),IMAGE_ICON,size,size,LR_SHARED);
            assert(icon);ICONINFO info;assert(GetIconInfo(icon,&info));BITMAP bitmap;
            assert(GetObjectW(info.hbmColor,sizeof bitmap,&bitmap));assert(bitmap.bmWidth==size && bitmap.bmHeight==size);
            DeleteObject(info.hbmColor);DeleteObject(info.hbmMask);
        }
    }
    return 0;
}
