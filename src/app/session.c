#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "session.h"

bool dc_app_restart_view(dc_level_view_t **view, const char *base_directory,
                         uint64_t seed, uint32_t run, char *err, uint32_t cap) {
    if (!view || !*view || !base_directory || !base_directory[0] || !run) {
        if (err && cap) snprintf(err, cap, "Invalid world restart");
        return false;
    }
    char path[928];
    int length = snprintf(path, sizeof(path), "%s_run_%u_XXXXXX",
                          base_directory, run);
    if (length < 0 || length > 900 || length >= (int)sizeof(path) ||
        !mkdtemp(path)) {
        if (err && cap) snprintf(err, cap, "Cannot create fresh world directory");
        return false;
    }
    bool saved = dc_level_view_destroy(*view, err, cap);
    *view = NULL;
    if (!saved) return false;
    *view = dc_level_view_create(path, seed, err, cap);
    return *view != NULL;
}
