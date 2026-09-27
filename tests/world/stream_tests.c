#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dungeoncraft/stream.h"

static int g_pass = 0;
static int g_fail = 0;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static bool wait_result(dc_streamer_t *stream, dc_stream_result_t *result) {
    struct timespec pause_time = {0, 1000000};
    for (int i = 0; i < 3000; ++i) {
        if (dc_stream_poll(stream, result)) return true;
        nanosleep(&pause_time, NULL);
    }
    return false;
}

static void test_worker_generates_saves_and_reloads_chunk(void) {
    char directory[] = "build/stream_test_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_streamer_t *stream = dc_stream_create(directory, 1234, 2);
    ASSERT_TRUE(stream != NULL);
    dc_chunk_coord_t coord = {-1, 0};
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, coord, 7));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.generation, 7u);
    ASSERT_TRUE(result.chunk != NULL);
    ASSERT_EQ(result.chunk->coord.x, -1);
    result.chunk->cells[10 * DC_CHUNK_SIDE + 63].material = 42;
    result.chunk->face_velocity[10 * DC_CHUNK_SIDE + 63] =
        (dc_face_velocity_t){ .x = 1.25f, .y = -0.5f };
    result.chunk->marker_count = 1;
    result.chunk->markers[0] = (dc_marker_t){ .x_fp = 63 << 16,
        .y_fp = 10 << 16, .id = 123, .kind = DC_MARKER_INSIDE };
    ASSERT_TRUE(dc_stream_request_save(stream, result.chunk, 7));
    dc_stream_result_release(&result);
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(stream, coord, 9));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.generation, 9u);
    ASSERT_EQ(result.chunk->cells[10 * DC_CHUNK_SIDE + 63].material, 42);
    ASSERT_TRUE(result.chunk->face_velocity[10 * DC_CHUNK_SIDE + 63].x == 1.25f);
    ASSERT_TRUE(result.chunk->face_velocity[10 * DC_CHUNK_SIDE + 63].y == -0.5f);
    ASSERT_EQ(result.chunk->marker_count, 1u);
    ASSERT_EQ(result.chunk->markers[0].id, 123u);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

static void test_shutdown_flushes_queued_saves(void) {
    char directory[] = "build/stream_shutdown_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_streamer_t *stream = dc_stream_create(directory, 55, 16);
    ASSERT_TRUE(stream != NULL);
    dc_chunk_t chunk = {0};
    for (int i = 0; i < 16; ++i) {
        chunk.coord.x = i;
        chunk.cells[0].material = (uint16_t)(100 + i);
        ASSERT_TRUE(dc_stream_request_save(stream, &chunk, (uint64_t)(i + 1)));
    }
    dc_stream_destroy(stream);
    stream = dc_stream_create(directory, 55, 2);
    ASSERT_TRUE(stream != NULL);
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, (dc_chunk_coord_t){15, 0}, 99));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[0].material, 115);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

static void test_resident_slot_round_trip_through_worker(void) {
    char directory[] = "build/stream_residency_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_streamer_t *stream = dc_stream_create(directory, 98, 2);
    ASSERT_TRUE(stream != NULL);
    dc_chunk_table_t table = {0};
    ASSERT_TRUE(dc_chunk_table_init(&table, 1));
    dc_chunk_coord_t a = {-2, 0}, b = {3, 0};
    uint32_t slot = 0;
    uint64_t generation = 0;
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_chunk_table_begin_load(&table, a, &slot, &generation));
    ASSERT_TRUE(dc_stream_request_load(stream, a, generation));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_TRUE(dc_chunk_table_finish_load(&table, slot, result.generation));
    result.chunk->cells[1].material = 77;
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&table, slot));
    ASSERT_TRUE(dc_chunk_table_set_active(&table, slot, false));
    ASSERT_TRUE(dc_chunk_table_begin_save(&table, slot, &generation));
    ASSERT_TRUE(dc_stream_request_save(stream, result.chunk, generation));
    dc_stream_result_release(&result);
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    ASSERT_TRUE(dc_chunk_table_finish_save(&table, slot, result.generation));
    ASSERT_TRUE(dc_chunk_table_evict(&table, slot, 0));
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_chunk_table_begin_load(&table, b, &slot, &generation));
    ASSERT_TRUE(dc_stream_request_load(stream, b, generation));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_TRUE(dc_chunk_table_finish_load(&table, slot, result.generation));
    ASSERT_TRUE(dc_chunk_table_set_active(&table, slot, false));
    ASSERT_TRUE(dc_chunk_table_evict(&table, slot, 0));
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_chunk_table_begin_load(&table, a, &slot, &generation));
    ASSERT_TRUE(dc_stream_request_load(stream, a, generation));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[1].material, 77);
    ASSERT_TRUE(dc_chunk_table_finish_load(&table, slot, result.generation));
    dc_stream_result_release(&result);
    dc_chunk_table_destroy(&table);
    dc_stream_destroy(stream);
    PASS();
}

static void test_worker_loads_procedural_basin(void) {
    char directory[] = "build/stream_basin_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_streamer_t *stream = dc_stream_create(directory, 314, 2);
    ASSERT_TRUE(stream != NULL);
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, (dc_chunk_coord_t){2, 0}, 1));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[40 * DC_CHUNK_SIDE].fluid_mass, DC_FLUID_FULL);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

static void test_legacy_chunk_loads_with_zero_velocity(void) {
    char directory[] = "build/stream_legacy_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    char path[256];
    ASSERT_TRUE(snprintf(path, sizeof(path),
        "%s/chunk_0000000000000000_0000000000000000.bin", directory) > 0);
    FILE *file = fopen(path, "wb");
    ASSERT_TRUE(file != NULL);
    struct {
        char magic[4];
        uint32_t version;
        int64_t x, y;
        uint64_t seed;
    } header = { .magic = {'D', 'C', 'C', '1'}, .version = 2, .seed = 98 };
    dc_chunk_t *old = calloc(1, sizeof(*old));
    ASSERT_TRUE(old != NULL);
    old->cells[10].fluid_mass = DC_FLUID_FULL;
    old->marker_count = 1;
    old->markers[0].id = 42;
    ASSERT_TRUE(fwrite(&header, sizeof(header), 1, file) == 1);
    ASSERT_TRUE(fwrite(old->cells, sizeof(old->cells), 1, file) == 1);
    ASSERT_TRUE(fwrite(&old->marker_count, sizeof(old->marker_count), 1, file) == 1);
    ASSERT_TRUE(fwrite(old->markers, sizeof(dc_marker_t), 1, file) == 1);
    ASSERT_TRUE(fclose(file) == 0);
    free(old);
    dc_streamer_t *stream = dc_stream_create(directory, 98, 2);
    ASSERT_TRUE(stream != NULL);
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, (dc_chunk_coord_t){0, 0}, 1));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[10].fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(result.chunk->markers[0].id, 42u);
    ASSERT_TRUE(result.chunk->face_velocity[10].x == 0.0f);
    ASSERT_TRUE(result.chunk->face_velocity[10].y == 0.0f);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

static void test_version_four_grain_radius_migrates(void) {
    char directory[] = "build/stream_grain_v4_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    char path[256];
    ASSERT_TRUE(snprintf(path, sizeof(path),
        "%s/chunk_0000000000000000_0000000000000000.bin", directory) > 0);
    FILE *file = fopen(path, "wb");
    ASSERT_TRUE(file != NULL);
    struct {
        char magic[4];
        uint32_t version;
        int64_t x, y;
        uint64_t seed;
    } header = { .magic = {'D', 'C', 'C', '1'}, .version = 4, .seed = 73 };
    dc_chunk_t *old = calloc(1, sizeof(*old));
    ASSERT_TRUE(old != NULL);
    old->cells[10].material = DC_MATERIAL_SAND;
    dc_chunk_particle_init(&old->particles[10], (dc_chunk_coord_t){0, 0},
                           10u, DC_MATERIAL_SAND);
    old->particles[10].grain_fp = DC_FLUID_FULL / 2u;
    old->particle_count = 1u;
    uint32_t index = 10u;
    ASSERT_TRUE(fwrite(&header, sizeof(header), 1, file) == 1);
    ASSERT_TRUE(fwrite(old->cells, sizeof(old->cells), 1, file) == 1);
    ASSERT_TRUE(fwrite(&old->marker_count, sizeof(old->marker_count), 1, file) == 1);
    ASSERT_TRUE(fwrite(old->face_velocity, sizeof(old->face_velocity), 1, file) == 1);
    ASSERT_TRUE(fwrite(&old->particle_count, sizeof(old->particle_count), 1, file) == 1);
    ASSERT_TRUE(fwrite(&index, sizeof(index), 1, file) == 1);
    ASSERT_TRUE(fwrite(&old->particles[index], sizeof(dc_mpm_particle_t), 1, file) == 1);
    ASSERT_TRUE(fclose(file) == 0);
    free(old);
    dc_streamer_t *stream = dc_stream_create(directory, 73, 2);
    ASSERT_TRUE(stream != NULL);
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, (dc_chunk_coord_t){0, 0}, 1));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->particles[index].grain_fp, DC_SAND_GRAIN_RADIUS_FP);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

int main(void) {
    RUN(test_worker_generates_saves_and_reloads_chunk);
    RUN(test_shutdown_flushes_queued_saves);
    RUN(test_resident_slot_round_trip_through_worker);
    RUN(test_worker_loads_procedural_basin);
    RUN(test_legacy_chunk_loads_with_zero_velocity);
    RUN(test_version_four_grain_radius_migrates);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
