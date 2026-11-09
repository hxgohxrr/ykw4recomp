#ifndef YKW4_LAUNCHER_BINDING_H
#define YKW4_LAUNCHER_BINDING_H
#include <stddef.h>
#include <wchar.h>
#ifdef _WIN32
typedef wchar_t LauncherPathChar;
#else
typedef char LauncherPathChar;
#endif

int launcher_binding_identity(unsigned char digest[32]);
#ifndef _WIN32
int launcher_binding_identity_mode(unsigned mode, unsigned char digest[32]);
#endif
int launcher_binding_firmware_identity(const unsigned char* firmware, size_t size, unsigned char digest[32]);
int launcher_binding_kind(const LauncherPathChar* path);
int launcher_binding_required(const LauncherPathChar* installed);
int launcher_binding_seal(const LauncherPathChar* path, LauncherPathChar* error, size_t capacity);
void* launcher_binding_load(const char* path, char* error, size_t capacity);
void launcher_binding_release(void);
#endif
