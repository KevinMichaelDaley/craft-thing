#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dungeoncraft/generate.h"
#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static uint64_t now_us(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000u + (uint64_t)time.tv_nsec / 1000u;
}

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
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    uint32_t *pixels = calloc(DC_CHUNK_CELLS, sizeof(*pixels));
    ASSERT_TRUE(pixels != NULL);
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, DC_CHUNK_CELLS, err, sizeof(err)));
    uint32_t rendered = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        if (generated->particles[i].mass_fp && pixels[i] == 0xff40c8e0u) ++rendered;
    ASSERT_EQ(rendered, generated->particle_count);
    free(pixels);
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
    ASSERT_TRUE(left && right);
    right->coord.x = 1;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    uint64_t upload_start = now_us();
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    uint64_t paint_start = now_us();
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 63, 5, 0, DC_MATERIAL_SAND,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 63, 5, 0, DC_MATERIAL_SAND,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_EQ(left->particle_count, 1u);
    dc_mpm_particle_t original = left->particles[5 * DC_CHUNK_SIDE + 63];
    ASSERT_EQ(original.mass_fp, DC_FLUID_FULL);
    ASSERT_EQ(original.material, DC_MATERIAL_SAND);
    ASSERT_TRUE(original.id_lo || original.id_hi);
    uint64_t transfer_start = now_us();
    dc_gpu_transfer_t transfer = { .from_x = 63, .from_y = 5,
        .to_x = 64, .to_y = 5, .kind = DC_GPU_TRANSFER_PARTICLE };
    ASSERT_TRUE(dc_gpu_queue_transfer(gpu, transfer, err, sizeof(err)));
    dc_gpu_transfer_state_t state;
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    uint64_t transfer_end = now_us();
    printf("particle stages (sync wall us): upload=%llu paint+readback=%llu transfer=%llu\n",
           (unsigned long long)(paint_start - upload_start),
           (unsigned long long)(transfer_start - paint_start),
           (unsigned long long)(transfer_end - transfer_start));
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
    free(left); free(right);
    PASS();
}

static void test_granular_paint_reuses_and_erases_primary_slot(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    const uint16_t materials[] = { DC_MATERIAL_SAND, DC_MATERIAL_DIRT,
                                   DC_MATERIAL_GRAVEL };
    for (uint32_t i = 0; i < 3; ++i) {
        ASSERT_TRUE(dc_gpu_paint_material(gpu, 10, 10, 0, materials[i],
                                          err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
        ASSERT_EQ(chunk->particle_count, 1u);
        ASSERT_EQ(chunk->particles[10 * DC_CHUNK_SIDE + 10].material, materials[i]);
        ASSERT_EQ(chunk->particles[10 * DC_CHUNK_SIDE + 10].mass_fp, DC_FLUID_FULL);
    }
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 10, 10, 0, DC_MATERIAL_AIR,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 0u);
    ASSERT_EQ(chunk->particles[10 * DC_CHUNK_SIDE + 10].mass_fp, 0u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_upload_rejects_inconsistent_particle_count(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t i = 1; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        chunk->particles[i].mass_fp = DC_FLUID_FULL;
    chunk->particle_count = DC_MPM_PARTICLES_PER_CHUNK;
    chunk->cells[0].material = DC_MATERIAL_SAND;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "does not match") != NULL);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_granular_tick_falls_and_reports_gpu_time(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
        chunk->cells[48 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    chunk->cells[8 * DC_CHUNK_SIDE + 20].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    dc_mpm_particle_t original = chunk->particles[8 * DC_CHUNK_SIDE + 20];
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    for (uint32_t i = 0; i < 24; ++i)
        ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_EQ(capture.stages[2].id, DC_GPU_STAGE_SAND);
    ASSERT_TRUE(capture.stages[2].gpu_ns > 0u);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) {
            ASSERT_EQ(chunk->particles[i].id_lo, original.id_lo);
            ASSERT_EQ(chunk->particles[i].mass_fp, original.mass_fp);
            ASSERT_TRUE(chunk->particles[i].y_fp > original.y_fp + 2 * (int32_t)DC_FLUID_FULL);
            ASSERT_TRUE(chunk->particles[i].y_fp < 48 * (int32_t)DC_FLUID_FULL);
            ++found;
        }
    ASSERT_EQ(found, 1u);
    printf("granular GPU stage: %llu ns\n",
           (unsigned long long)capture.stages[2].gpu_ns);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_granular_motion_crosses_seam_with_stable_mass(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    right->coord.x = 1;
    left->cells[12 * DC_CHUNK_SIDE + 63].material = DC_MATERIAL_GRAVEL;
    dc_chunk_seed_particles(left);
    dc_mpm_particle_t original = left->particles[12 * DC_CHUNK_SIDE + 63];
    left->particles[12 * DC_CHUNK_SIDE + 63].vx_fp = 2 * (int32_t)DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    for (uint32_t i = 0; i < 8; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_EQ(left->particle_count + right->particle_count, 1u);
    ASSERT_EQ(right->particle_count, 1u);
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (right->particles[i].mass_fp) {
            ASSERT_EQ(right->particles[i].id_lo, original.id_lo);
            ASSERT_EQ(right->particles[i].id_hi, original.id_hi);
            ASSERT_EQ(right->particles[i].mass_fp, original.mass_fp);
            ++found;
        }
    ASSERT_EQ(found, 1u);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static bool run_granular_pile(const dc_chunk_t *initial, dc_chunk_t *result,
                             char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, cap))
        return false;
    bool okay = dc_gpu_upload_chunk(gpu, 0, initial, err, cap) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, cap);
    for (uint32_t tick = 0; tick < 80 && okay; ++tick)
        okay = dc_gpu_tick_step(gpu, err, cap);
    if (okay) okay = dc_gpu_download_chunk(gpu, 0, result, err, cap);
    dc_gpu_destroy(gpu);
    return okay;
}

static void test_mixed_pile_conserves_mass_and_replays(void) {
    char err[256] = {0};
    dc_chunk_t *initial = calloc(1, sizeof(*initial));
    dc_chunk_t *first = calloc(1, sizeof(*first));
    dc_chunk_t *second = calloc(1, sizeof(*second));
    ASSERT_TRUE(initial && first && second);
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
        initial->cells[45 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    const uint16_t kinds[3] = { DC_MATERIAL_SAND, DC_MATERIAL_DIRT,
                                DC_MATERIAL_GRAVEL };
    for (uint32_t y = 8; y < 11; ++y)
        for (uint32_t x = 20; x < 23; ++x)
            initial->cells[y * DC_CHUNK_SIDE + x].material = kinds[(x + y) % 3u];
    dc_chunk_seed_particles(initial);
    ASSERT_EQ(initial->particle_count, 9u);
    ASSERT_TRUE(run_granular_pile(initial, first, err, sizeof(err)));
    ASSERT_TRUE(run_granular_pile(initial, second, err, sizeof(err)));
    ASSERT_EQ(first->particle_count, 9u);
    ASSERT_EQ(second->particle_count, 9u);
    uint64_t mass = 0;
    uint32_t moved = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
        const dc_mpm_particle_t *particle = &first->particles[i];
        mass += particle->mass_fp;
        if (!particle->mass_fp) continue;
        ASSERT_TRUE(particle->y_fp < 45 * (int32_t)DC_FLUID_FULL);
        if (particle->y_fp > 20 * (int32_t)DC_FLUID_FULL) ++moved;
    }
    ASSERT_EQ(mass, 9u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(moved > 0);
    ASSERT_EQ(memcmp(first->particles, second->particles,
                     sizeof(first->particles)), 0);
    free(initial); free(first); free(second);
    PASS();
}

static void test_grain_collides_with_gpu_body(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
        chunk->cells[35 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    chunk->cells[12 * DC_CHUNK_SIDE + 30].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    dc_gpu_body_t body = { .x_fp = 28 << 16, .y_fp = 31 << 16,
        .width = 5, .height = 4, .id = 7, .active = 1 };
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 40; ++tick)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp)
            ASSERT_TRUE(chunk->particles[i].y_fp < 31 * (int32_t)DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_two_slot_collision_resolves_by_stable_id(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    uint32_t row = 15u * DC_CHUNK_SIDE;
    for (uint32_t x = 20; x <= 22; ++x)
        chunk->cells[row + x].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    chunk->particles[row + 20].vx_fp = 2 * (int32_t)DC_FLUID_FULL;
    chunk->particles[row + 22].vx_fp = -2 * (int32_t)DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 3u);
    uint32_t active = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        active += chunk->particles[i].mass_fp != 0u;
    ASSERT_EQ(active, 3u);
    uint32_t secondary = 0;
    uint32_t occupied_cell = UINT32_MAX;
    for (uint32_t i = DC_CHUNK_CELLS; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) {
            ++secondary;
            occupied_cell = i - DC_CHUNK_CELLS;
        }
    ASSERT_TRUE(secondary > 0u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, occupied_cell % DC_CHUNK_SIDE,
                                     occupied_cell / DC_CHUNK_SIDE, 0,
                                     DC_MATERIAL_AIR, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    ASSERT_EQ(chunk->particles[occupied_cell].mass_fp, 0u);
    ASSERT_EQ(chunk->particles[occupied_cell + DC_CHUNK_CELLS].mass_fp, 0u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_resident_window_reports_mpm_gpu_time(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_gpu_create(&gpu, 384, 256, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 6; ++x) {
            uint32_t slot = y * 6u + x;
            dc_generate_chunk(314, (dc_chunk_coord_t){(int64_t)x - 1,
                                                       (int64_t)y - 1}, chunk);
            ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(gpu, x, y, slot, err, sizeof(err)));
        }
    }
    dc_gpu_tick_capture_t capture = {0};
    ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_TRUE(capture.stages[2].gpu_ns > 0u);
    printf("resident 384x256 granular GPU stage: %llu ns\n",
           (unsigned long long)capture.stages[2].gpu_ns);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_wet_grain_moves_through_eulerian_water(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t y = 12; y < 32; ++y)
        for (uint32_t x = 12; x < 48; ++x)
            chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    chunk->cells[15 * DC_CHUNK_SIDE + 20].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    for (uint32_t i = 0; i < 8; ++i)
        ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) {
            ASSERT_TRUE(chunk->particles[i].y_fp > 16 * (int32_t)DC_FLUID_FULL);
            ++found;
        }
    ASSERT_EQ(found, 1u);
    ASSERT_TRUE(capture.stages[2].gpu_ns > 0u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_water_drag_exchanges_momentum_with_grain(void) {
    char err[256] = {0};
    dc_chunk_t *wet = calloc(1, sizeof(*wet));
    dc_chunk_t *reference = calloc(1, sizeof(*reference));
    ASSERT_TRUE(wet && reference);
    for (uint32_t y = 8; y < 56; ++y)
        for (uint32_t x = 8; x < 56; ++x) {
            uint32_t index = y * DC_CHUNK_SIDE + x;
            wet->cells[index].fluid_mass = DC_FLUID_FULL;
            wet->face_velocity[index].x = 1.0f;
        }
    memcpy(reference, wet, sizeof(*wet));
    uint32_t source = 30 * DC_CHUNK_SIDE + 30;
    wet->cells[source].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(wet);
    dc_gpu_t *gpu = NULL, *reference_gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_create(&reference_gpu, 64, 64,
                              "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, wet, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, wet, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(reference_gpu, 0, reference, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(reference_gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(reference_gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(reference_gpu, 0, reference, err, sizeof(err)));
    ASSERT_TRUE(wet->face_velocity[source].x < reference->face_velocity[source].x - 0.01f);
    ASSERT_EQ(wet->particle_count, 1u);
    uint32_t moving = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (wet->particles[i].mass_fp && wet->particles[i].vx_fp > 0) ++moving;
    ASSERT_EQ(moving, 1u);
    dc_gpu_destroy(gpu);
    dc_gpu_destroy(reference_gpu);
    free(wet); free(reference);
    PASS();
}

static void test_coupled_flow_crosses_chunk_seam(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    right->coord.x = 1;
    for (uint32_t y = 8; y < 56; ++y)
        for (uint32_t x = 0; x < 64; ++x) {
            uint32_t index = y * DC_CHUNK_SIDE + x;
            left->cells[index].fluid_mass = DC_FLUID_FULL;
            right->cells[index].fluid_mass = DC_FLUID_FULL;
            left->face_velocity[index].x = 1.5f;
            right->face_velocity[index].x = 1.5f;
        }
    uint32_t source = 30 * DC_CHUNK_SIDE + 63;
    left->cells[source].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(left);
    uint32_t id = left->particles[source].id_lo;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    for (uint32_t tick = 0; tick < 8; ++tick)
        ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_EQ(left->particle_count + right->particle_count, 1u);
    ASSERT_EQ(right->particle_count, 1u);
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (right->particles[i].mass_fp) {
            ASSERT_EQ(right->particles[i].id_lo, id);
            ASSERT_EQ(right->particles[i].mass_fp, DC_FLUID_FULL);
            ++found;
        }
    ASSERT_EQ(found, 1u);
    ASSERT_TRUE(capture.stages[2].gpu_ns > 0u);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_closed_wet_grain_momentum_balance(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    dc_chunk_t *still = calloc(1, sizeof(*still));
    ASSERT_TRUE(chunk && still);
    for (uint32_t y = 8; y < 56; ++y)
        for (uint32_t x = 8; x < 56; ++x) {
            uint32_t index = y * DC_CHUNK_SIDE + x;
            chunk->cells[index].fluid_mass = DC_FLUID_FULL;
            chunk->face_velocity[index].x = 1.0f;
        }
    uint32_t source = 30 * DC_CHUNK_SIDE + 30;
    chunk->cells[source].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    memcpy(still, chunk, sizeof(*still));
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        still->face_velocity[i].x = 0.0f;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_EQ(vkResetCommandBuffer(gpu->command, 0), VK_SUCCESS);
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    ASSERT_EQ(vkBeginCommandBuffer(gpu->command, &begin), VK_SUCCESS);
    dc_gpu_record_mpm(gpu);
    ASSERT_EQ(vkEndCommandBuffer(gpu->command), VK_SUCCESS);
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    ASSERT_EQ(vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE), VK_SUCCESS);
    ASSERT_EQ(vkQueueWaitIdle(gpu->queue), VK_SUCCESS);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    double grain_momentum = 0.0, water_delta = 0.0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp)
            grain_momentum += (double)chunk->particles[i].mass_fp / DC_FLUID_FULL *
                              (double)chunk->particles[i].vx_fp / DC_FLUID_FULL;
    for (uint32_t y = 8; y < 56; ++y)
        for (uint32_t x = 8; x < 56; ++x) {
            uint32_t index = y * DC_CHUNK_SIDE + x;
            water_delta += chunk->face_velocity[index].x - 1.0;
        }
    printf("closed MPM/water x momentum: grain=%g water=%g residual=%g\n",
           grain_momentum, water_delta, grain_momentum + water_delta);
    ASSERT_TRUE(grain_momentum > 0.0);
    ASSERT_TRUE(grain_momentum + water_delta < 4.0 / DC_FLUID_FULL &&
                grain_momentum + water_delta > -4.0 / DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, still, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, still, err, sizeof(err)));
    ASSERT_EQ(still->particle_count, 1u);
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (still->particles[i].mass_fp)
            ASSERT_TRUE(abs(still->particles[i].vx_fp) < 655);
    dc_gpu_destroy(gpu);
    free(chunk);
    free(still);
    PASS();
}

static void test_dirt_binds_small_water_dose_without_losing_mass(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    uint32_t source = 24 * DC_CHUNK_SIDE + 24;
    chunk->cells[source].material = DC_MATERIAL_DIRT;
    chunk->cells[source].fluid_mass = DC_FLUID_FULL / 16u;
    chunk->cells[source - 1].material = DC_MATERIAL_STONE;
    chunk->cells[source + 1].material = DC_MATERIAL_STONE;
    chunk->cells[source - DC_CHUNK_SIDE].material = DC_MATERIAL_STONE;
    chunk->cells[source + DC_CHUNK_SIDE].material = DC_MATERIAL_STONE;
    dc_chunk_seed_particles(chunk);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    uint32_t moisture = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) {
            moisture = chunk->particles[i].flags & DC_MPM_MOISTURE_MASK;
            ASSERT_TRUE((chunk->particles[i].flags & DC_MPM_MUD_FLAG) != 0u);
        }
    ASSERT_TRUE(moisture >= DC_FLUID_FULL / 32u);
    ASSERT_EQ(chunk->cells[source].fluid_mass + moisture, DC_FLUID_FULL / 16u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_water_brush_keeps_dirt_particle(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 20, 20, 0, DC_MATERIAL_DIRT,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    uint32_t source = 20 * DC_CHUNK_SIDE + 20;
    uint32_t id = chunk->particles[source].id_lo;
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 20, 20, 0, DC_MATERIAL_WATER,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    ASSERT_EQ(chunk->cells[source].material, DC_MATERIAL_DIRT);
    ASSERT_EQ(chunk->cells[source].fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(chunk->particles[source].id_lo, id);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_drying_returns_bound_water_to_eulerian_cell(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    uint32_t source = 24 * DC_CHUNK_SIDE + 24;
    chunk->cells[source].material = DC_MATERIAL_DIRT;
    chunk->cells[source - 1].material = DC_MATERIAL_STONE;
    chunk->cells[source + 1].material = DC_MATERIAL_STONE;
    chunk->cells[source - DC_CHUNK_SIDE].material = DC_MATERIAL_STONE;
    chunk->cells[source + DC_CHUNK_SIDE].material = DC_MATERIAL_STONE;
    dc_chunk_seed_particles(chunk);
    chunk->particles[source].flags = DC_MPM_MUD_FLAG | 4096u;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    uint32_t moisture = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp)
            moisture = chunk->particles[i].flags & DC_MPM_MOISTURE_MASK;
    ASSERT_EQ(chunk->cells[source].fluid_mass, 64u);
    ASSERT_EQ(moisture, 4032u);
    ASSERT_EQ(chunk->cells[source].fluid_mass + moisture, 4096u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static uint64_t combined_water_mass(const dc_chunk_t *chunk) {
    uint64_t mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        mass += chunk->cells[i].fluid_mass;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].material == DC_MATERIAL_DIRT)
            mass += chunk->particles[i].flags & DC_MPM_MOISTURE_MASK;
    return mass;
}

static void test_coupled_materials_conserve_water_across_seam_and_stream(void) {
    char directory[] = "build/mud_stream_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    right->coord.x = 1;
    uint32_t dirt = 20 * DC_CHUNK_SIDE + 63;
    left->cells[dirt].material = DC_MATERIAL_DIRT;
    for (uint32_t y = 20; y < 27; ++y)
        for (uint32_t x = 60; x < 64; ++x)
            left->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    for (uint32_t y = 20; y < 27; ++y)
        for (uint32_t x = 0; x < 8; ++x)
            right->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    right->cells[22 * DC_CHUNK_SIDE + 3].material = DC_MATERIAL_SAND;
    right->cells[22 * DC_CHUNK_SIDE + 5].material = DC_MATERIAL_GRAVEL;
    dc_chunk_seed_particles(left);
    dc_chunk_seed_particles(right);
    left->particles[dirt].flags = DC_MPM_MUD_FLAG | DC_FLUID_FULL / 16u;
    left->particles[dirt].vx_fp = 2 * (int32_t)DC_FLUID_FULL;
    uint32_t dirt_id = left->particles[dirt].id_lo;
    uint64_t initial_water = combined_water_mass(left) + combined_water_mass(right);
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    for (uint32_t tick = 0; tick < 12; ++tick)
        ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
    ASSERT_EQ(left->particle_count + right->particle_count, 3u);
    ASSERT_EQ(combined_water_mass(left) + combined_water_mass(right), initial_water);
    ASSERT_TRUE(capture.stages[1].gpu_ns > 0u && capture.stages[2].gpu_ns > 0u);
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (right->particles[i].mass_fp && right->particles[i].id_lo == dirt_id) {
            ASSERT_EQ(right->particles[i].material, DC_MATERIAL_DIRT);
            ASSERT_TRUE((right->particles[i].flags & DC_MPM_MOISTURE_MASK) != 0u);
            ++found;
        }
    ASSERT_EQ(found, 1u);
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
    ASSERT_EQ(combined_water_mass(result.chunk), combined_water_mass(right));
    ASSERT_EQ(memcmp(result.chunk->particles, right->particles,
                     sizeof(right->particles)), 0);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_dry_and_saturated_dirt_yield_differently(void) {
    char err[256] = {0};
    dc_chunk_t *dry = calloc(1, sizeof(*dry));
    dc_chunk_t *wet = calloc(1, sizeof(*wet));
    ASSERT_TRUE(dry && wet);
    wet->coord.x = 1;
    uint32_t source = 20 * DC_CHUNK_SIDE + 30;
    dry->cells[source].material = DC_MATERIAL_DIRT;
    wet->cells[source].material = DC_MATERIAL_DIRT;
    dc_chunk_seed_particles(dry);
    dc_chunk_seed_particles(wet);
    dry->particles[source].x_fp += DC_FLUID_FULL / 4;
    wet->particles[source].x_fp += DC_FLUID_FULL / 4;
    dry->particles[source].deformation[0] = 0.8f;
    dry->particles[source].deformation[3] = 0.8f;
    dry->particles[source].deformation[1] = 0.15f;
    dry->particles[source].deformation[2] = 0.15f;
    memcpy(wet->particles[source].deformation, dry->particles[source].deformation,
           sizeof(dry->particles[source].deformation));
    wet->particles[source].flags = DC_MPM_MUD_FLAG | DC_MPM_MOISTURE_CAP;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, dry, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, wet, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, dry, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, wet, err, sizeof(err)));
    ASSERT_EQ(dry->particle_count, 1u);
    ASSERT_EQ(wet->particle_count, 1u);
    dc_mpm_particle_t *dry_grain = NULL, *wet_grain = NULL;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
        if (dry->particles[i].mass_fp) dry_grain = &dry->particles[i];
        if (wet->particles[i].mass_fp) wet_grain = &wet->particles[i];
    }
    ASSERT_TRUE(dry_grain && wet_grain);
    ASSERT_EQ(dry_grain->flags & DC_MPM_MUD_FLAG, 0u);
    ASSERT_TRUE((wet_grain->flags & DC_MPM_MUD_FLAG) != 0u);
    ASSERT_TRUE(dry_grain->deformation[0] != wet_grain->deformation[0] ||
                dry_grain->deformation[1] != wet_grain->deformation[1] ||
                dry_grain->vx_fp != wet_grain->vx_fp);
    dc_gpu_destroy(gpu);
    free(dry); free(wet);
    PASS();
}

static void test_free_sand_horizontal_velocity_has_tiny_decay(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    uint32_t source = 12 * DC_CHUNK_SIDE + 30;
    chunk->cells[source].material = DC_MATERIAL_SAND;
    dc_chunk_seed_particles(chunk);
    chunk->particles[source].vx_fp = DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, 1u);
    int32_t vx = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) vx = chunk->particles[i].vx_fp;
    ASSERT_TRUE(vx > (int32_t)(0.99 * DC_FLUID_FULL));
    ASSERT_TRUE(vx < (int32_t)(0.9995 * DC_FLUID_FULL));
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

int main(void) {
    printf("GPU particle capacity: %u records per chunk, %u primary cell slots\n",
           DC_MPM_PARTICLES_PER_CHUNK, DC_CHUNK_CELLS);
    RUN(test_generated_sand_has_stable_gpu_particles);
    RUN(test_painted_particle_crosses_seam_and_streams_once);
    RUN(test_granular_paint_reuses_and_erases_primary_slot);
    RUN(test_upload_rejects_inconsistent_particle_count);
    RUN(test_granular_tick_falls_and_reports_gpu_time);
    RUN(test_granular_motion_crosses_seam_with_stable_mass);
    RUN(test_mixed_pile_conserves_mass_and_replays);
    RUN(test_grain_collides_with_gpu_body);
    RUN(test_two_slot_collision_resolves_by_stable_id);
    RUN(test_resident_window_reports_mpm_gpu_time);
    RUN(test_wet_grain_moves_through_eulerian_water);
    RUN(test_water_drag_exchanges_momentum_with_grain);
    RUN(test_coupled_flow_crosses_chunk_seam);
    RUN(test_closed_wet_grain_momentum_balance);
    RUN(test_dirt_binds_small_water_dose_without_losing_mass);
    RUN(test_water_brush_keeps_dirt_particle);
    RUN(test_drying_returns_bound_water_to_eulerian_cell);
    RUN(test_coupled_materials_conserve_water_across_seam_and_stream);
    RUN(test_dry_and_saturated_dirt_yield_differently);
    RUN(test_free_sand_horizontal_velocity_has_tiny_decay);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
