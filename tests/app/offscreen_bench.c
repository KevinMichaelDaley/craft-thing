#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../src/app/offscreen.h"

enum { WORKSPACES = 16u, WORLD_TICKS = 24u };

static double seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

int main(void) {
    char err[256] = {0};
    dc_gpu_t *parent = NULL;
    dc_offscreen_t *workspaces[WORKSPACES] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk || !dc_gpu_create(&parent, 1920u, 1080u,
                                  "build/shaders/pattern.comp.spv",
                                  err, sizeof(err))) goto fail;
    for (uint32_t x = 0u; x < DC_CHUNK_SIDE; ++x)
        chunk->cells[47u * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    for (uint32_t y = 35u; y < 47u; ++y)
        for (uint32_t x = 20u; x < 44u; ++x)
            chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    for (uint32_t i = 0u; i < WORKSPACES; ++i) {
        workspaces[i] = dc_offscreen_create(parent,
            (dc_chunk_coord_t){0, 0}, err, sizeof(err));
        if (!workspaces[i] ||
            !dc_offscreen_capture(workspaces[i], chunk, err, sizeof(err)))
            goto fail;
    }
    const uint32_t camera_x[3] = {1u, 5u, 9u};
    const char *names[3] = {"near", "middle", "far"};
    for (uint32_t band = 0u; band < 3u; ++band) {
        double start = seconds();
        for (uint32_t tick = 0u; tick < WORLD_TICKS; ++tick)
            if (!dc_offscreen_update_batch(workspaces, WORKSPACES,
                (dc_chunk_coord_t){camera_x[band], 0},
                1.0 / 60.0, false, err, sizeof(err))) goto fail;
        printf("%s: %u active workspaces, %.3f ms per world tick\n",
               names[band], WORKSPACES,
               1000.0 * (seconds() - start) / WORLD_TICKS);
    }
    for (uint32_t i = 0u; i < WORKSPACES; ++i)
        dc_offscreen_destroy(workspaces[i]);
    dc_gpu_destroy(parent);
    free(chunk);
    return 0;
fail:
    fprintf(stderr, "offscreen benchmark failed: %s\n", err);
    for (uint32_t i = 0u; i < WORKSPACES; ++i)
        dc_offscreen_destroy(workspaces[i]);
    dc_gpu_destroy(parent);
    free(chunk);
    return 1;
}
