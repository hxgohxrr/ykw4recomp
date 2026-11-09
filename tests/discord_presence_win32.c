/* SPDX-License-Identifier: GPL-3.0-or-later */

#define DISCORD_PIPE_PATTERN L"\\\\?\\pipe\\YKW4-rpc-test-%d"
#define DISCORD_UPDATE_MS 100
#define DISCORD_RETRY_MS 50
#include "../src/nx/runtime/discord_presence.c"
#include <assert.h>
#include <stdlib.h>

static void server_send(HANDLE pipe, uint32_t op, const char* payload) {
    uint32_t header[] = {op, (uint32_t)strlen(payload)}; DWORD n;
    assert(WriteFile(pipe, header, 8, &n, NULL) && n == 8);
    assert(WriteFile(pipe, payload, header[1], &n, NULL) && n == header[1]);
}
static uint32_t server_read(HANDLE pipe, char* payload) {
    uint32_t header[2]; DWORD n;
    assert(ReadFile(pipe, header, 8, &n, NULL) && n == 8 && header[1] < 16000);
    uint32_t offset = 0;
    while (offset < header[1]) {
        assert(ReadFile(pipe, payload + offset, header[1] - offset, &n, NULL) && n);
        offset += n;
    }
    payload[offset] = 0; return header[0];
}
static DWORD WINAPI server(void* argument) {
    int dlc = (int)(intptr_t)argument;
    HANDLE pipe = CreateNamedPipeW(L"\\\\?\\pipe\\YKW4-rpc-test-0", PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, NULL);
    assert(pipe != INVALID_HANDLE_VALUE);
    assert(ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED);
    char payload[16000];
    assert(server_read(pipe, payload) == 0);
    assert(strstr(payload, "1561797893252317345"));
    server_send(pipe, 3, "hello");
    assert(server_read(pipe, payload) == 4 && !strcmp(payload, "hello"));
    server_send(pipe, 1, "{\"evt\":\"READY\"}");
    assert(server_read(pipe, payload) == 1);
    assert(strstr(payload, "SET_ACTIVITY") && strstr(payload, "YKW4Recomp"));
    if (dlc) assert(strstr(payload, "custom_expansion"));
    else assert(!strstr(payload, "assets"));
    assert(!!strstr(payload, "timestamps") == !dlc);
    assert(strstr(payload, "Zona \\\"prueba\\\""));
    assert(strstr(payload, "Starting YKW4"));
    assert(strstr(payload, "Project website") && strstr(payload, "https://yo-kaiporters.com"));
    cJSON* activity = cJSON_Parse(payload);
    assert(activity);
    cJSON_Delete(activity);
    server_send(pipe, 1, "{\"cmd\":\"SET_ACTIVITY\",\"data\":{\"text\":\"ERROR\"}}");
    discord_presence_update(1, 1);
    assert(server_read(pipe, payload) == 1 && strstr(payload, "In the in-game menu"));
    assert(discord_presence_connected());
    discord_presence_update(0, 1);
    assert(server_read(pipe, payload) == 1 && strstr(payload, "Playing YKW4"));
    assert(!discord_presence_toggle());
    assert(server_read(pipe, payload) == 1 && strstr(payload, "\"activity\":null"));
    assert(!GetPrivateProfileIntW(L"Discord", L"enabled", 1, config));
    DisconnectNamedPipe(pipe);
    assert(discord_presence_toggle());
    assert(ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED);
    assert(server_read(pipe, payload) == 0);
    server_send(pipe, 1, "{\"evt\":\"READY\"}");
    assert(server_read(pipe, payload) == 1 && strstr(payload, "Playing YKW4"));
    server_send(pipe, 1, "{\"evt\":\"ERROR\",\"data\":{\"message\":\"synthetic error\"}}");
    DisconnectNamedPipe(pipe); CloseHandle(pipe); return 0;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    char path[1400]; snprintf(path, sizeof path, "%sYKW4.discord.ini", argv[1]);
    wchar_t wide[32768]; assert(MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 32768));
    FILE* initial = _wfopen(wide, L"wb");
    const unsigned char bom[] = {0xff, 0xfe};
    assert(initial && fwrite(bom, 1, sizeof bom, initial) == sizeof bom);
    assert(!fclose(initial));
    assert(WritePrivateProfileStringW(L"Discord", L"enabled", L"1", wide));
    assert(WritePrivateProfileStringW(L"Discord", L"normal_image", NULL, wide));
    assert(WritePrivateProfileStringW(L"Discord", L"dlc_image", L"custom_expansion", wide));
    for (int dlc = 0; dlc < 2; dlc++) {
        assert(WritePrivateProfileStringW(L"Discord", L"context", L"Zona \"prueba\"", wide));
        assert(WritePrivateProfileStringW(L"Discord", L"show_time", dlc ? L"0" : L"1", wide));
        HANDLE mock = CreateThread(NULL, 0, server, (void*)(intptr_t)dlc, 0, NULL); assert(mock);
        discord_presence_start(argv[1], dlc);
        assert(WaitForSingleObject(mock, 12000) == WAIT_OBJECT_0);
        discord_presence_stop(); CloseHandle(mock);
    }
    assert(WritePrivateProfileStringW(L"Discord", L"enabled", L"0", wide));
    discord_presence_start(argv[1], 0);
    Sleep(100);
    assert(worker && !discord_presence_enabled() && !discord_presence_connected());
    discord_presence_stop();

    assert(WritePrivateProfileStringW(L"Discord", L"enabled", L"1", wide));
    discord_presence_start(argv[1], 0); Sleep(100); discord_presence_stop();
    char unicode[513];
    assert(WritePrivateProfileStringW(L"Discord", L"context", L"\u00e1\u00f1\u754c", wide));
    activity_state(unicode, sizeof unicode);
    assert(strstr(unicode, "\xc3\xa1\xc3\xb1\xe7\x95\x8c"));
    puts("discord_presence: handshake, ping/pong, DLC, JSON events, live stages, toggle/clear/reconnect, Unicode and offline shutdown passed");
    return 0;
}
