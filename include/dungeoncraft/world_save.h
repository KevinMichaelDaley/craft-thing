#ifndef DUNGEONCRAFT_WORLD_SAVE_H
#define DUNGEONCRAFT_WORLD_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#define DC_WORLD_BODY_RECORD "rigid_bodies.bin"
#define DC_WORLD_PLAYER_RECORD "player_inventory.bin"

typedef struct dc_world_save dc_world_save_t;

/** Open one writer and finish publishing any committed save before loading. */
dc_world_save_t *dc_world_save_open(const char *directory, char *err, uint32_t cap);

/** Create the private directory for a new save transaction. */
bool dc_world_save_begin(dc_world_save_t *save, char *err, uint32_t cap);

/** Return the canonical directory or the active staging directory. */
const char *dc_world_save_directory(const dc_world_save_t *save, bool staging);

/** Commit the staged snapshot once all of its writers have finished. */
bool dc_world_save_commit(dc_world_save_t *save, char *err, uint32_t cap);

/** Discard an uncommitted transaction and release the world writer. */
void dc_world_save_close(dc_world_save_t *save);

#endif
