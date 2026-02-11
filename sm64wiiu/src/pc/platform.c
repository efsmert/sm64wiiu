#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "platform.h"

#ifdef TARGET_WII_U
#include <sys/stat.h>
#endif

// Returns lowercase in-place for platform-independent case-folded comparisons.
char *sys_strlwr(char *src) {
    unsigned char *p = (unsigned char *) src;
    while (*p != '\0') {
        *p = (unsigned char) tolower(*p);
        p++;
    }
    return src;
}

// Allocates and copies a C string.
char *sys_strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, src, len);
    }
    return copy;
}

// Portable case-insensitive compare used by virtual filesystem lookups.
int sys_strcasecmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    int result = 0;
    while ((result = tolower(*p1) - tolower(*p2)) == 0) {
        if (*p1 == '\0') {
            break;
        }
        p1++;
        p2++;
    }
    return result;
}

const char *sys_file_extension(const char *fpath) {
    const char *fname = sys_file_name(fpath);
    const char *dot = strrchr(fname, '.');
    if (dot == NULL || dot == fname || dot[1] == '\0') {
        return NULL;
    }
    return dot + 1;
}

const char *sys_file_name(const char *fpath) {
    const char *sep1 = strrchr(fpath, '/');
    const char *sep2 = strrchr(fpath, '\\');
    const char *sep = sep1 > sep2 ? sep1 : sep2;
    return sep == NULL ? fpath : sep + 1;
}

void sys_swap_backslashes(char *buffer) {
    while (*buffer != '\0') {
        if (*buffer == '\\') {
            *buffer = '/';
        }
        buffer++;
    }
}

#ifdef TARGET_WII_U
// Wii U homebrew convention: store user data under the app folder on SD.
// This matches `sdcard://wiiu/apps/sm64wiiu/` (mapped to `/vol/external01/wiiu/apps/sm64wiiu/`).
static const char *wiiu_default_write_path(void) {
    static char path[SYS_MAX_PATH];

    if (path[0] != '\0') {
        return path;
    }

    // `pc_main.c` is responsible for mounting SD and creating this directory before `fs_init()`.
    snprintf(path, sizeof(path), "%s", "/vol/external01/wiiu/apps/sm64wiiu");
    return path;
}
#endif

const char *sys_user_path(void) {
#ifdef TARGET_WII_U
    return wiiu_default_write_path();
#else
    return ".";
#endif
}

const char *sys_resource_path(void) {
#ifdef TARGET_WII_U
    // WUHB-bundled runtime assets (lang/mod content payload) are mounted here.
    return "/vol/content";
#endif
    return sys_exe_path_dir();
}

const char *sys_exe_path_file(void) {
    static char path[SYS_MAX_PATH];
    if (path[0] != '\0') {
        return path;
    }

    if (getcwd(path, sizeof(path)) == NULL) {
        snprintf(path, sizeof(path), "%s", ".");
    }
    return path;
}

const char *sys_exe_path_dir(void) {
    return sys_exe_path_file();
}

// Formats a fatal error and terminates process execution.
void sys_fatal(const char *fmt, ...) {
    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    fprintf(stderr, "FATAL ERROR:\n%s\n", msg);
    fflush(stderr);
    exit(1);
}
