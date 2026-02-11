#ifndef MOD_FS_H
#define MOD_FS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Wii U port note:
// Co-op DX uses a "modfs" URI scheme to read files out of imported zip mods.
// The Wii U port currently loads mods directly from the mounted virtual
// filesystem (SD card + /vol/content), so "modfs" URIs are not used.
//
// DynOS still links against these helpers, so provide a tiny compatibility
// layer:
// - `is_mod_fs_file()` always returns false (no modfs scheme on Wii U).
// - `mod_fs_read_file_from_uri()` loads via the mounted VFS when possible.

bool is_mod_fs_file(const char *path);
bool mod_fs_read_file_from_uri(const char *uri, void **out_buffer, uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif

