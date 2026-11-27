#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "dungeoncraft/terrain_flags.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/offscreen_internal.h"
#include "../../src/vulkan/gpu_internal.h"

enum { ROOMS = DC_CAMERA_WORKSPACE_LIMIT + 1u, WARMUP = 12u, SAMPLES = 60u };
static char err[256];

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static int order(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static bool seed_room(dc_level_view_t *v, dc_chunk_coord_t coord, bool grains) {
    uint32_t slot;
    if (!dc_chunk_table_find(&v->table, coord, &slot)) return false;
    dc_chunk_t chunk = {.coord = coord};
    for (unsigned y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (unsigned x = 0; x < DC_CHUNK_SIDE; ++x) {
            dc_cell_t *c = chunk.cells + y * DC_CHUNK_SIDE + x;
            if (!x || !y || x+1 == DC_CHUNK_SIDE || y+1 == DC_CHUNK_SIDE)
                *c = (dc_cell_t){.material=DC_MATERIAL_STONE, .flags=DC_CELL_LOADED_TERRAIN};
            else if (x >= 20 && x < 44 && y >= 35 && y < 47)
                c->fluid_mass = DC_FLUID_FULL;
            else if (grains && x >= 20 && x < 44 && y >= 24 && y < 30)
                c->material = DC_MATERIAL_SAND;
        }
    }
    return dc_gpu_upload_chunk(v->gpu, slot, &chunk, err, sizeof(err)) &&
        dc_chunk_table_mark_dirty(&v->table, slot);
}

static unsigned active_rooms(const dc_level_view_t *v, double *oldest) {
    unsigned count = 0;
    *oldest = v->world_seconds;
    for (const dc_level_offscreen_handoff_t *h = v->offscreen_handoffs; h; h = h->next)
        if (h->admitted && h->workspace->time.known) {
            ++count;
            if (h->workspace->time.seconds < *oldest) *oldest = h->workspace->time.seconds;
        }
    return count;
}

static bool frame(dc_level_view_t *v) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) return false;
    return dc_level_view_step_timed(v, 1.f/60.f, err, sizeof(err)) &&
        dc_level_view_step_timed(v, 1.f/60.f, err, sizeof(err)) &&
        dc_level_view_tick(v, err, sizeof(err));
}

static bool measure(dc_level_view_t *v, const char *band, unsigned pan_screens, bool *budget) {
    double setup = now(), oldest;
    for (unsigned i = 0; i < pan_screens * VIEW_CHUNKS_X; ++i)
        if (!dc_level_view_move(v, 1, 0)) return false;
    if (!dc_level_view_wait_visible(v, 300000u, err, sizeof(err))) return false;
    while (active_rooms(v, &oldest) < DC_CAMERA_WORKSPACE_LIMIT) {
        if (now() - setup > 300. || !dc_level_view_tick(v, err, sizeof(err))) return false;
    }
    if (!seed_room(v, v->origin, true)) return false;
    for (unsigned i = 0; i < WARMUP; ++i) if (!frame(v)) return false;
    if (vkQueueWaitIdle(v->gpu->queue) != VK_SUCCESS) return false;
    double samples[SAMPLES], start = now(), begin = v->world_seconds;
    for (unsigned i = 0; i < SAMPLES; ++i) {
        double before = now();
        if (!frame(v)) return false;
        samples[i] = 1000. * (now() - before);
    }
    if (vkQueueWaitIdle(v->gpu->queue) != VK_SUCCESS) return false;
    double mean = 1000. * (now() - start) / SAMPLES;
    qsort(samples, SAMPLES, sizeof(*samples), order);
    double p95 = samples[(95u*SAMPLES+99u)/100u-1u];
    unsigned owners = active_rooms(v, &oldest);
    printf("%s owners=%u setup_and_warmup_s=%.3f samples=%u mean_ms=%.3f p95_ms=%.3f max_ms=%.3f "
           "simulated_s=%.9f oldest_owner_lag_s=%.9f\n",
        band, owners, start-setup, SAMPLES, mean, p95, samples[SAMPLES-1u],
        v->world_seconds-begin, v->world_seconds-oldest);
    *budget &= mean <= 1000./30. && p95 <= 1000./30.;
    return owners == DC_CAMERA_WORKSPACE_LIMIT && oldest > begin &&
        v->world_seconds == begin + 2u * SAMPLES * (double)(1.f/60.f);
}

int main(void) {
    char directory[] = "build/offscreen_cache_bench_XXXXXX";
    if (!mkdtemp(directory)) return 1;
    double start = now();
    dc_level_view_t *v = dc_level_view_create(directory, 42, err, sizeof(err));
    if (!v) { fprintf(stderr, "create failed: %s\n", err); return 1; }
    bool okay = dc_level_view_set_spring_enabled(v, false), budget = true;
    while (okay && v->origin.y > -48) okay = dc_level_view_move(v, 0, -1);
    if (okay) okay = dc_level_view_wait_visible(v, 300000u, err, sizeof(err));
    for (unsigned i = 0; okay && i < ROOMS; ++i)
        okay = seed_room(v, (dc_chunk_coord_t){v->origin.x + i%5u, v->origin.y + i/5u}, false);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(v->gpu->physical, &props);
    printf("device=%s viewport=%ux%u grid=%ux%u swapchain=%ux%u resident_slots=%u initial_setup_s=%.3f "
           "steps_per_frame=2 step_hz=60 warmup_frames=%u low_latency=%s display=%s\n",
        props.deviceName, VIEW_WIDTH, VIEW_HEIGHT, v->gpu->width, v->gpu->height,
        v->gpu->swap_extent.width, v->gpu->swap_extent.height, v->table.capacity, now()-start,
        WARMUP, getenv("DC_LOW_LATENCY_PRESENT") ? getenv("DC_LOW_LATENCY_PRESENT") : "default",
        getenv("DISPLAY") ? getenv("DISPLAY") : "unset");
    if (okay) okay = measure(v, "near", 1, &budget) && measure(v, "middle", 1, &budget) &&
        measure(v, "far", 2, &budget);
    if (!okay) fprintf(stderr, "Offscreen benchmark fixture failed: %s\n", err);
    dc_level_view_discard(v);
    if (okay && !budget) fprintf(stderr, "Presented frames exceeded the 30 FPS mean or p95 budget\n");
    return !okay || !budget;
}
