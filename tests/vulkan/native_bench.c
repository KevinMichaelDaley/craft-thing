#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dungeoncraft/gpu.h"

static double seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    uint32_t width = 1920, height = 1080;
    if (argc == 3) {
        width = (uint32_t)strtoul(argv[1], NULL, 10);
        height = (uint32_t)strtoul(argv[2], NULL, 10);
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [width height]\n", argv[0]);
        return 2;
    }
    if (width < 512 || height < 512 || width > 4096 || height > 2160) {
        fprintf(stderr, "dimensions must be 512..4096 x 512..2160\n");
        return 2;
    }
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return 1;
    if (!dc_gpu_create(&gpu, width, height, "build/shaders/pattern.comp.spv",
                       err, sizeof(err))) goto fail;
    uint32_t tile_x = (width / DC_CHUNK_SIDE - 8u) / 2u;
    uint32_t tile_y = (height / DC_CHUNK_SIDE - 8u) / 2u;
    for (uint32_t slot = 0; slot < DC_GPU_CHUNK_SLOTS; ++slot) {
        uint32_t tx = slot % 8u, ty = slot / 8u;
        chunk->coord = (dc_chunk_coord_t){(int64_t)tx, (int64_t)ty};
        for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
            for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
                dc_cell_t *cell = &chunk->cells[y * DC_CHUNK_SIDE + x];
                cell->material = y == DC_CHUNK_SIDE - 1u ? DC_MATERIAL_STONE : 0;
                cell->fluid_mass = y > 24u && y < 48u ? DC_FLUID_FULL : 0;
            }
        if (!dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)) ||
            !dc_gpu_set_page(gpu, tile_x + tx, tile_y + ty, slot,
                             err, sizeof(err))) goto fail;
    }
    for (uint32_t i = 0; i < 3; ++i)
        if (!dc_gpu_tick_step(gpu, err, sizeof(err))) goto fail;
    dc_gpu_tick_capture_t capture = {0};
    if (!dc_gpu_tick_capture(gpu, &capture, err, sizeof(err))) goto fail;
    const uint32_t samples = 12;
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
           width, height, (uint64_t)width * height, DC_GPU_CHUNK_SLOTS,
           DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS);
    printf("tick_wall_ms=%.3f tick_rate_hz=%.2f\n",
           1000.0 * tick_elapsed / samples, samples / tick_elapsed);
    printf("tick_plus_render_wall_ms=%.3f frame_rate_hz=%.2f\n",
           1000.0 * frame_elapsed / samples, samples / frame_elapsed);
    printf("gpu_stage_ms rigid=%.3f fluid=%.3f granular=%.3f\n",
           capture.stages[0].gpu_ns / 1e6, capture.stages[1].gpu_ns / 1e6,
           capture.stages[2].gpu_ns / 1e6);
    dc_gpu_destroy(gpu);
    free(chunk);
    return 0;
fail:
    fprintf(stderr, "native bench failed: %s\n", err);
    dc_gpu_destroy(gpu);
    free(chunk);
    return 1;
}
