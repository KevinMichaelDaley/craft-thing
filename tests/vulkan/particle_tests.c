#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/generate.h"
#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_generated_sand_has_stable_gpu_particles(void) {
    dc_chunk_t *generated = calloc(1, sizeof(*generated));
    dc_chunk_t *replayed = calloc(1, sizeof(*replayed));
    ASSERT_TRUE(generated && replayed);
    dc_generate_chunk(314, (dc_chunk_coord_t){6, 0}, generated);
    dc_generate_chunk(314, (dc_chunk_coord_t){6, 0}, replayed);
    ASSERT_TRUE(generated->particle_count > 0);
    ASSERT_EQ(generated->particle_count, replayed->particle_count);
    uint64_t mass = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
        ASSERT_EQ(generated->particles[i].id_lo, replayed->particles[i].id_lo);
        ASSERT_EQ(generated->particles[i].id_hi, replayed->particles[i].id_hi);
        mass += generated->particles[i].mass_fp;
    }
    ASSERT_EQ(mass, (uint64_t)generated->particle_count * DC_FLUID_FULL);
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, generated, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, replayed, err, sizeof(err)));
    ASSERT_EQ(replayed->particle_count, generated->particle_count);
    ASSERT_EQ(memcmp(generated->particles, replayed->particles,
                     sizeof(generated->particles)), 0);
    dc_gpu_destroy(gpu);
    free(generated); free(replayed);
    PASS();
}

static void test_painted_particle_crosses_seam_and_streams_once(void) {
    char directory[] = "build/particle_stream_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    dc_chunk_t *saved = calloc(1, sizeof(*saved));
    ASSERT_TRUE(left && right && saved);
    right->coord.x = 1;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 63, 5, 0, DC_MATERIAL_SAND,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_EQ(left->particle_count, 1u);
    dc_mpm_particle_t original = left->particles[5 * DC_CHUNK_SIDE + 63];
    ASSERT_EQ(original.mass_fp, DC_FLUID_FULL);
    ASSERT_EQ(original.material, DC_MATERIAL_SAND);
    ASSERT_TRUE(original.id_lo || original.id_hi);
    dc_gpu_transfer_t transfer = { .from_x = 63, .from_y = 5,
        .to_x = 64, .to_y = 5, .kind = DC_GPU_TRANSFER_PARTICLE };
    ASSERT_TRUE(dc_gpu_queue_transfer(gpu, transfer, err, sizeof(err)));
    dc_gpu_transfer_state_t state;
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_EQ(left->particle_count, 0u);
    ASSERT_EQ(right->particle_count, 1u);
    ASSERT_EQ(right->particles[5 * DC_CHUNK_SIDE].id_lo, original.id_lo);
    ASSERT_EQ(right->particles[5 * DC_CHUNK_SIDE].id_hi, original.id_hi);
    ASSERT_EQ(right->particles[5 * DC_CHUNK_SIDE].mass_fp, original.mass_fp);
    ASSERT_EQ(right->particles[5 * DC_CHUNK_SIDE].deformation[0],
              original.deformation[0]);
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_EQ(right->particle_count, 1u);
    dc_streamer_t *stream = dc_stream_create(directory, 314, 8);
    ASSERT_TRUE(stream != NULL);
    ASSERT_TRUE(dc_stream_request_save(stream, right, 1));
    dc_stream_result_t result;
    while (!dc_stream_poll(stream, &result)) { }
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(stream, right->coord, 2));
    while (!dc_stream_poll(stream, &result)) { }
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->particle_count, 1u);
    ASSERT_EQ(result.chunk->particles[5 * DC_CHUNK_SIDE].id_lo, original.id_lo);
    ASSERT_EQ(result.chunk->particles[5 * DC_CHUNK_SIDE].mass_fp, original.mass_fp);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    dc_gpu_destroy(gpu);
    free(left); free(right); free(saved);
    PASS();
}

int main(void) {
    printf("GPU particle capacity: %u per chunk\n", DC_MPM_PARTICLES_PER_CHUNK);
    RUN(test_generated_sand_has_stable_gpu_particles);
    RUN(test_painted_particle_crosses_seam_and_streams_once);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
