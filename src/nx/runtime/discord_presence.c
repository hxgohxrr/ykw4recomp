/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "discord_presence.h"
#include "vendor/cjson/cJSON.h"

static HANDLE worker, stop, wake;
static wchar_t config[32768];
static int has_dlc;
static long long started;
static LONG enabled, phase, connected;
#ifndef DISCORD_UPDATE_MS
#define DISCORD_UPDATE_MS 15000
#endif
#ifndef DISCORD_RETRY_MS
#define DISCORD_RETRY_MS 5000
#endif
#ifndef DISCORD_PIPE_PATTERN
#define DISCORD_PIPE_PATTERN L"\\\\?\\pipe\\discord-ipc-%d"
#endif

static void text(const wchar_t* key, const wchar_t* fallback, char* out, int size) {
    wchar_t wide[129];
    GetPrivateProfileStringW(L"Discord", key, fallback, wide, 129, config);
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL)) out[0] = 0;
}
static void escape(const char* in, char* out) {
    const unsigned char* p = (const unsigned char*)in;
    for (; *p; p++) {
        if (*p < 32) { sprintf(out, "\\u%04x", *p); out += 6; }
        else { if (*p == '"' || *p == '\\') *out++ = '\\'; *out++ = *p; }
    }
    *out = 0;
}

static int transfer(HANDLE pipe, void* buffer, DWORD count, int write) {
    unsigned char* p = buffer;
    while (count) {
        OVERLAPPED ov = {0}; ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!ov.hEvent) return 0;
        DWORD done = 0;
        BOOL ok = write ? WriteFile(pipe, p, count, &done, &ov) : ReadFile(pipe, p, count, &done, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            HANDLE events[] = {stop, ov.hEvent};
            if (WaitForMultipleObjects(2, events, FALSE, 1000) != WAIT_OBJECT_0 + 1) {
                CancelIoEx(pipe, &ov);
                GetOverlappedResult(pipe, &ov, &done, TRUE);
                CloseHandle(ov.hEvent); return 0;
            }
            ok = GetOverlappedResult(pipe, &ov, &done, FALSE);
        }
        CloseHandle(ov.hEvent);
        if (!ok || !done) return 0;
        p += done; count -= done;
    }
    return 1;
}
static int send_frame(HANDLE pipe, uint32_t opcode, const char* body, uint32_t length) {
    uint32_t header[] = {opcode, length};
    return transfer(pipe, header, sizeof header, 1) && transfer(pipe, (void*)body, length, 1);
}
static int receive(HANDLE pipe, int* ready) {
    uint32_t header[2]; char body[65537];
    if (!transfer(pipe, header, sizeof header, 0) || header[1] > sizeof body - 1 ||
        !transfer(pipe, body, header[1], 0)) return 0;
    body[header[1]] = 0;
    if (header[0] == 2) return 0;
    if (header[0] == 3) return send_frame(pipe, 4, body, header[1]);
    if (header[0] != 1) return 0;
    cJSON* message = cJSON_ParseWithLengthOpts(body, (size_t)header[1] + 1, NULL, 1);
    if (!message) return 0;
    const cJSON* event = cJSON_GetObjectItemCaseSensitive(message, "evt");
    int failed = cJSON_IsString(event) && !strcmp(event->valuestring, "ERROR");
    if (cJSON_IsString(event) && !strcmp(event->valuestring, "READY")) *ready = 1;
    cJSON_Delete(message);
    return !failed;
}

static void clear_activity(HANDLE pipe) {
    char clear[256];
    snprintf(clear, sizeof clear, "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"exit\",\"args\":{\"pid\":%lu,\"activity\":null}}", GetCurrentProcessId());
    send_frame(pipe, 1, clear, (uint32_t)strlen(clear));
}

static void activity_state(char* state, size_t size) {
    char context[513];
    static const char* const stages[] = {"Starting YKW4", "Playing YKW4", "In the in-game menu"};
    text(L"context", L"", context, sizeof context);
    LONG current = InterlockedCompareExchange(&phase, 0, 0);
    snprintf(state, size, "%s%s%s", stages[current], context[0] ? " | " : "", context);
    size_t length = strlen(state);
    if (length > 128) {
        length = 128;
        while (((unsigned char)state[length] & 0xc0) == 0x80) --length;
        state[length] = 0;
    }
}

static DWORD WINAPI presence_thread(void* unused) {
    (void)unused;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    unsigned nonce = 0; ULONGLONG next_send = 0, handshake_deadline = 0;
    int ready = 0;
    DWORD delay = 0;
    for (;;) {
        HANDLE events[] = {stop, wake};
        if (WaitForMultipleObjects(2, events, FALSE, delay) == WAIT_OBJECT_0) break;
        delay = pipe == INVALID_HANDLE_VALUE ? DISCORD_RETRY_MS : 200;
        if (!InterlockedCompareExchange(&enabled, 0, 0)) {
            if (pipe != INVALID_HANDLE_VALUE) {
                clear_activity(pipe);
                CloseHandle(pipe);
                pipe = INVALID_HANDLE_VALUE;
            }
            InterlockedExchange(&connected, 0);
            continue;
        }
        if (pipe == INVALID_HANDLE_VALUE) {
            for (int i = 0; i < 10; i++) {
                wchar_t name[128]; swprintf(name, 128, DISCORD_PIPE_PATTERN, i);
                pipe = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
                if (pipe != INVALID_HANDLE_VALUE) break;
            }
            if (pipe == INVALID_HANDLE_VALUE) continue;
            char app[128], handshake[256]; text(L"application_id", L"1561797893252317345", app, sizeof app);
            size_t length = strlen(app);
            if (!length || length > 20 || strspn(app, "0123456789") != length) goto disconnected;
            snprintf(handshake, sizeof handshake, "{\"v\":1,\"client_id\":\"%s\"}", app);
            if (!send_frame(pipe, 0, handshake, (uint32_t)strlen(handshake))) goto disconnected;
            ready = 0; next_send = 0; handshake_deadline = GetTickCount64() + 5000;
            delay = 200;
        }
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) goto disconnected;
        if (available && !receive(pipe, &ready)) goto disconnected;
        if (!ready && GetTickCount64() > handshake_deadline) goto disconnected;
        InterlockedExchange(&connected, ready);
        if (ready && GetTickCount64() >= next_send) {
            char details[513], state[513], image[513], ed[3079], es[3079], ei[3079], payload[15000], assets[3300] = "", timestamps[128] = "";
            text(L"details", L"YKW4Recomp", details, sizeof details);
            if (!details[0]) snprintf(details, sizeof details, "YKW4Recomp");
            activity_state(state, sizeof state);
            text(has_dlc ? L"dlc_image" : L"normal_image", L"", image, sizeof image);
            escape(details, ed); escape(state, es); escape(image, ei);
            if (GetPrivateProfileIntW(L"Discord", L"show_time", 1, config))
                snprintf(timestamps, sizeof timestamps, ",\"timestamps\":{\"start\":%lld}", started);
            if (image[0]) snprintf(assets, sizeof assets, ",\"assets\":{\"large_image\":\"%s\",\"large_text\":\"YKW4Recomp\"}", ei);
            snprintf(payload, sizeof payload,
                "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%u\",\"args\":{\"pid\":%lu,\"activity\":{\"type\":0,"
                "\"details\":\"%s\",\"state\":\"%s\"%s%s,"
                "\"buttons\":[{\"label\":\"Project website\",\"url\":\"https://yo-kaiporters.com\"}]}}}",
                ++nonce, GetCurrentProcessId(), ed, es, assets, timestamps);
            if (!send_frame(pipe, 1, payload, (uint32_t)strlen(payload))) goto disconnected;
            next_send = GetTickCount64() + DISCORD_UPDATE_MS;
        }
        continue;
disconnected:
        CloseHandle(pipe); pipe = INVALID_HANDLE_VALUE;
        InterlockedExchange(&connected, 0);
        delay = DISCORD_RETRY_MS;
    }
    if (pipe != INVALID_HANDLE_VALUE) {

        clear_activity(pipe); CloseHandle(pipe);
    }
    InterlockedExchange(&connected, 0);
    return 0;
}
void discord_presence_start(const char* directory, int dlc) {
    char path[1400]; snprintf(path, sizeof path, "%sYKW4.discord.ini", directory);
    if (worker || !MultiByteToWideChar(CP_UTF8, 0, path, -1, config, 32768)) return;
    InterlockedExchange(&enabled, GetPrivateProfileIntW(L"Discord", L"enabled", 1, config) != 0);
    InterlockedExchange(&phase, 0);
    has_dlc = dlc; started = (long long)time(NULL);
    stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (stop && wake) worker = CreateThread(NULL, 0, presence_thread, NULL, 0, NULL);
    if (!worker && stop) { CloseHandle(stop); stop = NULL; }
    if (!worker && wake) { CloseHandle(wake); wake = NULL; }
}
void discord_presence_stop(void) {
    if (!worker) return;
    SetEvent(stop); WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker); CloseHandle(stop); CloseHandle(wake); worker = stop = wake = NULL;
    InterlockedExchange(&enabled, 0);
}
void discord_presence_update(int menu, int rendered) {
    LONG next = menu ? 2 : rendered ? 1 : 0;
    if (InterlockedExchange(&phase, next) != next && wake) SetEvent(wake);
}
int discord_presence_enabled(void) { return InterlockedCompareExchange(&enabled, 0, 0) != 0; }
int discord_presence_connected(void) { return InterlockedCompareExchange(&connected, 0, 0) != 0; }
int discord_presence_toggle(void) {
    if (!worker) return 0;
    int next = !(InterlockedXor(&enabled, 1) & 1);
    WritePrivateProfileStringW(L"Discord", L"enabled", next ? L"1" : L"0", config);
    SetEvent(wake);
    return next;
}
