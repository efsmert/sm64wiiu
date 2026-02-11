#include "mod_fs.h"

#include <stdlib.h>

#include "pc/fs/fs.h"

bool is_mod_fs_file(const char *path) {
    (void)path;
    return false;
}

bool mod_fs_read_file_from_uri(const char *uri, void **out_buffer, uint32_t *out_size) {
    uint64_t size64 = 0;
    void *buf = NULL;

    if (out_buffer == NULL || out_size == NULL || uri == NULL) {
        return false;
    }

    *out_buffer = NULL;
    *out_size = 0;

    buf = fs_load_file(uri, &size64);
    if (buf == NULL) {
        return false;
    }

    if (size64 > UINT32_MAX) {
        free(buf);
        return false;
    }

    *out_buffer = buf;
    *out_size = (uint32_t)size64;
    return true;
}

