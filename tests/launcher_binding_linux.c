#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "launcher_binding.h"

static unsigned identity_version = 1;
int launcher_binding_identity(unsigned char digest[32]) {
    memset(digest, (int)identity_version, 32);
    return identity_version != 0;
}
int launcher_binding_identity_mode(unsigned mode, unsigned char digest[32]) {
    return launcher_binding_identity(digest) ? (mode ? (int)mode : 2) : 0;
}

static void copy_file(const char* source, const char* target) {
    FILE* input = fopen(source, "rb");
    FILE* output = fopen(target, "wb");
    assert(input && output);
    unsigned char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, input))) assert(fwrite(buffer, 1, count, output) == count);
    assert(!ferror(input));
    assert(!fclose(input));
    assert(!fclose(output));
}

int main(int argc, char** argv) {
    assert(argc == 2);
    char home[] = "/tmp/ykw4-binding-test-XXXXXX";
    assert(mkdtemp(home));
    assert(!setenv("HOME", home, 1));
    char module[4096], corrupt[4096], link[4096], key[4096], folder[4096], error[1024];
    snprintf(module, sizeof module, "%s/game.so", home);
    snprintf(corrupt, sizeof corrupt, "%s/corrupt.so", home);
    snprintf(link, sizeof link, "%s/link.so", home);
    snprintf(folder, sizeof folder, "%s/.ykw4-binding", home);
    snprintf(key, sizeof key, "%s/.ykw4-binding/key-v2", home);
    assert(launcher_binding_required(module) == 1);
    copy_file(argv[1], module);
    assert(launcher_binding_kind(module) == 0);
    assert(launcher_binding_required(module) == 0);
    void* handle = launcher_binding_load(module, error, sizeof error);
    assert(handle);
    int (*value)(void) = (int (*)(void))dlsym(handle, "binding_fixture_value");
    assert(value && value() == 427);
    assert(!dlclose(handle));
    identity_version = 0;
    assert(!launcher_binding_seal(module, error, sizeof error));
    assert(launcher_binding_kind(module) == 0);
    identity_version = 1;
    assert(launcher_binding_seal(module, error, sizeof error));
    assert(launcher_binding_kind(module) == 1);
    struct stat info;
    assert(!stat(key, &info) && (info.st_mode & 0777) == 0600);
    assert(launcher_binding_seal(module, error, sizeof error));
    handle = launcher_binding_load(module, error, sizeof error);
    assert(handle);
    value = (int (*)(void))dlsym(handle, "binding_fixture_value");
    assert(value && value() == 427);
    launcher_binding_release();
    launcher_binding_release();
    identity_version = 2;
    assert(!launcher_binding_load(module, error, sizeof error));
    assert(!launcher_binding_seal(module, error, sizeof error));
    identity_version = 1;
    assert(launcher_binding_load(module, error, sizeof error));
    launcher_binding_release();
    copy_file(module, corrupt);
    FILE* file = fopen(corrupt, "r+b");
    assert(file && !fseek(file, 40, SEEK_SET));
    int byte = fgetc(file);
    assert(byte != EOF && !fseek(file, 40, SEEK_SET));
    assert(fputc(byte ^ 1, file) != EOF && !fclose(file));
    assert(!launcher_binding_load(corrupt, error, sizeof error));
    assert(!symlink(module, link));
    assert(launcher_binding_kind(link) < 0);
    assert(!launcher_binding_seal(link, error, sizeof error));
    assert(!chmod(key, 0644));
    assert(!launcher_binding_load(module, error, sizeof error));
    assert(!chmod(key, 0600));
    assert(!unlink(key));
    assert(!launcher_binding_load(module, error, sizeof error));
    assert(!unlink(link) && !unlink(module) && !unlink(corrupt));
    assert(!rmdir(folder) && !rmdir(home));
    puts("Linux module binding: legacy, sealing, load, identity mismatch, corruption, permissions and cleanup OK");
    return 0;
}
