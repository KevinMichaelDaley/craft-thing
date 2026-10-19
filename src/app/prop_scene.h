#ifndef DUNGEONCRAFT_APP_PROP_SCENE_H
#define DUNGEONCRAFT_APP_PROP_SCENE_H

#include "level.h"

/** Create a world using the package's optional surrounding-ground companion. */
dc_level_view_t *dc_prop_scene_create(const char *directory, uint64_t seed,
                                      const char *path, char *err, uint32_t cap);

/** Load one terrain asset centered on world chunk (0,0), without stepping physics. */
bool dc_prop_scene_load(dc_level_view_t *view, const char *path, char *err, uint32_t cap);
/** Exercise actual scene placement, brush editing, paused frames and physics in a window. */
int dc_prop_scene_smoke(const char *path);

#endif
