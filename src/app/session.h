#ifndef DUNGEONCRAFT_APP_SESSION_H
#define DUNGEONCRAFT_APP_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "level.h"

/** Save the current world and start a fresh procedural run under a new directory. */
bool dc_app_restart_view(dc_level_view_t **view, const char *base_directory,
                         uint64_t seed, uint32_t run, char *err, uint32_t cap);

#endif
