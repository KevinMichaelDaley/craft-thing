#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dungeoncraft/gpu.h"
#ifdef DC_QUARTER_NATIVE_VIEW
#include "../../src/app/view_config.h"
#endif
#include "native_scene.h"

static double seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
#ifdef DC_QUARTER_NATIVE_VIEW
    uint32_t width = SIM_WIDTH, height = SIM_HEIGHT;
#else
    uint32_t width = 1920, height = 1080;
#endif
    uint32_t samples = 12;
    if (argc == 3 || argc == 4) {
        width = (uint32_t)strtoul(argv[1], NULL, 10);
        height = (uint32_t)strtoul(argv[2], NULL, 10);
        if (argc == 4) samples = (uint32_t)strtoul(argv[3], NULL, 10);
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [width height [samples]]\n", argv[0]);
        return 2;
    }
    if (width < 256 || height < 128 || width > 4096 || height > 2160) {
        fprintf(stderr, "dimensions must be 256..4096 x 128..2160\n");
        return 2;
    }
    uint32_t resident = native_scene_count(width, height);
    if (resident > DC_GPU_CHUNK_SLOTS || !samples || samples > 10000u) {
        fprintf(stderr, "scene exceeds %u chunk slots or samples outside 1..10000\n",
                DC_GPU_CHUNK_SLOTS);
        return 2;
    }
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return 1;
    if (!dc_gpu_create(&gpu, width, height, "build/shaders/pattern.comp.spv",
                       err, sizeof(err))) goto fail;
    uint32_t particles = 0;
    uint64_t water = 0;
    for (uint32_t slot = 0; slot < resident; ++slot) {
        native_scene_chunk(chunk, slot, width, height);
        particles += chunk->particle_count;
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
            water += chunk->cells[i].fluid_mass;
        if (!dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)) ||
            !dc_gpu_set_page(gpu, (uint32_t)chunk->coord.x, (uint32_t)chunk->coord.y, slot,
                             err, sizeof(err))) goto fail;
    }
    for (uint32_t i = 0; i < 3; ++i)
        if (!dc_gpu_tick_step(gpu, err, sizeof(err))) goto fail;
    dc_gpu_tick_capture_t capture = {0};
    uint64_t stage_ns[3] = {0};
    for (uint32_t i = 0; i < samples; ++i) {
        if (!dc_gpu_tick_capture(gpu, &capture, err, sizeof(err))) goto fail;
        for (uint32_t stage = 0; stage < 3; ++stage)
            stage_ns[stage] += capture.stages[stage].gpu_ns;
    }
    double start = seconds();
    for (uint32_t i = 0; i < samples; ++i)
        if (!dc_gpu_tick_step(gpu, err, sizeof(err))) goto fail;
    double tick_elapsed = seconds() - start;
    start = seconds();
    for (uint32_t i = 0; i < samples; ++i) {
        if (!dc_gpu_tick_step(gpu, err, sizeof(err)) ||
            !dc_gpu_render_chunks(gpu, err, sizeof(err))) goto fail;
    }
    double frame_elapsed = seconds() - start;
    printf("grid=%ux%u pixels=%" PRIu64 " resident_chunks=%u resident_cells=%u\n",
           width, height, (uint64_t)width * height, resident,
           resident * DC_CHUNK_CELLS);
    printf("samples=%u initial_grains=%u initial_water_cells=%.2f\n",
           samples, particles, (double)water / DC_FLUID_FULL);
    printf("tick_wall_ms=%.3f tick_rate_hz=%.2f\n",
           1000.0 * tick_elapsed / samples, samples / tick_elapsed);
    printf("tick_plus_render_wall_ms=%.3f frame_rate_hz=%.2f\n",
           1000.0 * frame_elapsed / samples, samples / frame_elapsed);
    printf("gpu_stage_ms rigid=%.3f fluid=%.3f granular=%.3f\n",
           stage_ns[0] / (1e6 * samples), stage_ns[1] / (1e6 * samples),
           stage_ns[2] / (1e6 * samples));
    dc_gpu_destroy(gpu);
    free(chunk);
    return 0;
fail:
    fprintf(stderr, "native bench failed: %s\n", err);
    dc_gpu_destroy(gpu);
    free(chunk);
    return 1;
}
