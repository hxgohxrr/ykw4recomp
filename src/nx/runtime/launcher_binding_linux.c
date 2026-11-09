#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include "launcher_binding.h"

static const unsigned char magic[16] = {'Y','K','W','4','B','I','N','D',2,'L','N','X',0,0,0,0};
static void* loaded_module;
static int loaded_fd = -1;
static int registered_cleanup;

static int digest_bytes(const void* bytes, size_t size, unsigned char digest[32]) {
    unsigned count = 0;
    return EVP_Digest(bytes, size, digest, &count, EVP_sha256(), NULL) && count == 32;
}

static int private_key(unsigned char key[32], int create) {
    const char* home = getenv("HOME");
    if (!home || home[0] != '/') return 0;
    char folder[4096], path[4096];
    if (snprintf(folder, sizeof folder, "%s/.ykw4-binding", home) >= (int)sizeof folder ||
        snprintf(path, sizeof path, "%s/key-v2", folder) >= (int)sizeof path) return 0;
    if (create && mkdir(folder, 0700) && errno != EEXIST) return 0;
    struct stat info;
    if (lstat(folder, &info) || !S_ISDIR(info.st_mode) || info.st_uid != getuid() || (info.st_mode & 077)) return 0;
    int file = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file < 0 && errno == ENOENT && create) {
        unsigned char secret[64] = "YKW4KEY2", identity[32];
        int mode = launcher_binding_identity_mode(0, identity);
        OPENSSL_cleanse(identity, sizeof identity);
        if (!mode || RAND_bytes(secret + 32, 32) != 1) return 0;
        secret[8] = (unsigned char)mode;
        file = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (file >= 0) {
            int ok = write(file, secret, sizeof secret) == sizeof secret && !fsync(file);
            close(file);
            if (!ok) unlink(path);
        }
        OPENSSL_cleanse(secret, sizeof secret);
        file = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    }
    if (file < 0) return 0;
    unsigned char input[64], stored[64] = {0};
    int ok = !fstat(file, &info) && S_ISREG(info.st_mode) && info.st_uid == getuid() &&
        !(info.st_mode & 077) && info.st_size == 64 && read(file, stored, 64) == 64 &&
        !memcmp(stored, "YKW4KEY2", 8) && (stored[8] == 1 || stored[8] == 2);
    if (ok) {
        memcpy(input, stored + 32, 32);
        ok = launcher_binding_identity_mode(stored[8], input + 32) && digest_bytes(input, sizeof input, key);
    }
    close(file);
    OPENSSL_cleanse(input, sizeof input);
    OPENSSL_cleanse(stored, sizeof stored);
    return ok;
}

static int elf_kind(const unsigned char* bytes, size_t size) {
    Elf64_Ehdr header;
    if (size < sizeof header) return -1;
    memcpy(&header, bytes, sizeof header);
    return !memcmp(header.e_ident, ELFMAG, SELFMAG) && header.e_ident[EI_CLASS] == ELFCLASS64 &&
        header.e_ident[EI_DATA] == ELFDATA2LSB && header.e_ident[EI_VERSION] == EV_CURRENT &&
        header.e_type == ET_DYN && header.e_machine == EM_X86_64 && header.e_ehsize == sizeof header &&
        header.e_phentsize == sizeof(Elf64_Phdr) && header.e_phnum && header.e_phoff <= size &&
        header.e_phnum <= (size - header.e_phoff) / sizeof(Elf64_Phdr) ? 0 : -1;
}

static unsigned char* read_module(const char* path, size_t* size) {
    int file = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    struct stat info;
    unsigned char* bytes = NULL;
    if (file >= 0 && !fstat(file, &info) && S_ISREG(info.st_mode) && info.st_size >= 16 && info.st_size <= 1024 * 1024 * 1024) {
        *size = (size_t)info.st_size;
        bytes = malloc(*size);
        size_t offset = 0;
        while (bytes && offset < *size) {
            ssize_t count = read(file, bytes + offset, *size - offset);
            if (count <= 0) { free(bytes); bytes = NULL; break; }
            offset += (size_t)count;
        }
    }
    if (file >= 0) close(file);
    return bytes;
}

int launcher_binding_kind(const char* path) {
    size_t size = 0;
    unsigned char* bytes = read_module(path, &size);
    if (!bytes) return -1;
    int kind = !memcmp(bytes, magic, sizeof magic) ? (size > 44 ? 1 : -1) : elf_kind(bytes, size);
    free(bytes);
    return kind;
}

int launcher_binding_required(const char* path) {
    struct stat info;
    if (lstat(path, &info)) return errno == ENOENT ? 1 : -1;
    return launcher_binding_kind(path);
}

static unsigned char* decrypt_module(const unsigned char* bytes, size_t size, size_t* length) {
    if (size <= 44 || memcmp(bytes, magic, sizeof magic)) return NULL;
    unsigned char key[32];
    if (!private_key(key, 0)) return NULL;
    *length = size - 44;
    unsigned char* plain = malloc(*length + 16);
    EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
    int count = 0, tail = 0;
    int ok = plain && context && EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), NULL, key, bytes + 16) &&
        EVP_DecryptUpdate(context, NULL, &count, bytes, 16) &&
        EVP_DecryptUpdate(context, plain, &count, bytes + 28, (int)*length) &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, 16, (void*)(bytes + size - 16)) &&
        EVP_DecryptFinal_ex(context, plain + count, &tail) && elf_kind(plain, *length) == 0;
    EVP_CIPHER_CTX_free(context);
    OPENSSL_cleanse(key, sizeof key);
    if (!ok) { if (plain) OPENSSL_clear_free(plain, *length + 16); return NULL; }
    return plain;
}

int launcher_binding_seal(const char* path, char* error, size_t capacity) {
    size_t size = 0, length = 0;
    unsigned char* bytes = read_module(path, &size);
    unsigned char *sealed = NULL, *plain = NULL, key[32] = {0};
    EVP_CIPHER_CTX* context = NULL;
    char* temporary = NULL;
    int file = -1, ok = 0;
    if (!bytes) goto done;
    if (!memcmp(bytes, magic, sizeof magic)) {
        plain = decrypt_module(bytes, size, &length);
        ok = plain != NULL;
        goto done;
    }
    if (elf_kind(bytes, size) || size > 1024 * 1024 * 1024 - 44 || !private_key(key, 1)) goto done;
    sealed = malloc(size + 60);
    context = EVP_CIPHER_CTX_new();
    if (!sealed || !context) goto done;
    memcpy(sealed, magic, sizeof magic);
    int count = 0, tail = 0;
    if (RAND_bytes(sealed + 16, 12) != 1 ||
        !EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), NULL, key, sealed + 16) ||
        !EVP_EncryptUpdate(context, NULL, &count, sealed, 16) ||
        !EVP_EncryptUpdate(context, sealed + 28, &count, bytes, (int)size) ||
        !EVP_EncryptFinal_ex(context, sealed + 28 + count, &tail) ||
        !EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, 16, sealed + 28 + size)) goto done;
    if (asprintf(&temporary, "%s.bind-XXXXXX", path) < 0) { temporary = NULL; goto done; }
    file = mkstemp(temporary);
    if (file < 0) goto done;
    size_t offset = 0;
    while (offset < size + 44) {
        ssize_t written = write(file, sealed + offset, size + 44 - offset);
        if (written <= 0) goto done;
        offset += (size_t)written;
    }
    if (fsync(file) || rename(temporary, path)) goto done;
    ok = 1;
done:
    if (file >= 0) close(file);
    if (temporary && !ok) unlink(temporary);
    free(temporary);
    if (bytes) OPENSSL_clear_free(bytes, size);
    if (plain) OPENSSL_clear_free(plain, length + 16);
    free(sealed);
    EVP_CIPHER_CTX_free(context);
    OPENSSL_cleanse(key, sizeof key);
    if (!ok && capacity) snprintf(error, capacity, "No se pudo vincular el módulo. Comprueba la identidad, la clave privada y los permisos. La instalación anterior no se ha modificado.");
    return ok;
}

void launcher_binding_release(void) {
    if (loaded_module) dlclose(loaded_module);
    if (loaded_fd >= 0) close(loaded_fd);
    loaded_module = NULL;
    loaded_fd = -1;
}

void* launcher_binding_load(const char* path, char* error, size_t capacity) {
    size_t size = 0, length = 0;
    unsigned char* bytes = read_module(path, &size);
    if (!bytes) goto failed;
    if (memcmp(bytes, magic, sizeof magic)) {
        int valid = elf_kind(bytes, size) == 0;
        free(bytes);
        void* module = valid ? dlopen(path, RTLD_NOW | RTLD_LOCAL) : NULL;
        if (!module && capacity) snprintf(error, capacity, "No se pudo cargar el módulo ELF o sus dependencias.");
        return module;
    }
    unsigned char* plain = decrypt_module(bytes, size, &length);
    free(bytes);
    if (!plain || loaded_module) { if (plain) OPENSSL_clear_free(plain, length + 16); goto failed; }
    int file = memfd_create("ykw4-module", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    size_t offset = 0;
    while (file >= 0 && offset < length) {
        ssize_t written = write(file, plain + offset, length - offset);
        if (written <= 0) break;
        offset += (size_t)written;
    }
    OPENSSL_clear_free(plain, length + 16);
    if (file < 0) goto failed;
    if (offset != length || fcntl(file, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) { close(file); goto failed; }
    char descriptor[64];
    snprintf(descriptor, sizeof descriptor, "/proc/self/fd/%d", file);
    void* module = dlopen(descriptor, RTLD_NOW | RTLD_LOCAL);
    if (!module || (!registered_cleanup && atexit(launcher_binding_release))) {
        if (module) dlclose(module);
        close(file);
        goto failed;
    }
    registered_cleanup = 1;
    loaded_module = module;
    loaded_fd = file;
    return module;
failed:
    if (capacity) snprintf(error, capacity, "Módulo dañado, dependencias ausentes o vinculado a otra instalación/usuario. Prepara de nuevo en otra carpeta. No se han cambiado las partidas.");
    return NULL;
}
