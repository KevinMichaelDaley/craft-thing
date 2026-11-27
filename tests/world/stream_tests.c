#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dungeoncraft/stream.h"
#include "dungeoncraft/world_save.h"

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
    ASSERT_TRUE(!result.time.known);
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
    ASSERT_TRUE(!result.time.known);
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

typedef struct {
    char magic[4];
    uint32_t version;
    int64_t x, y;
    uint64_t seed;
    uint32_t payload_version, reserved;
    double completed_seconds;
} timed_chunk_fixture_header_t;

static bool write_chunk_clock_fixture(const char *path) {
    const timed_chunk_fixture_header_t header = {
        .magic = {'D', 'C', 'C', '1'}, .version = 12, .seed = 98,
        .payload_version = 5, .completed_seconds = 42.125};
    dc_cell_t cells[DC_CHUNK_CELLS] = {0};
    dc_face_velocity_t faces[DC_CHUNK_CELLS] = {0};
    const uint32_t zero = 0;
    cells[10].fluid_mass = DC_FLUID_FULL;
    faces[10].x = 1.25f;
    const void *parts[] = {&header, cells, &zero, faces, &zero};
    const size_t sizes[] = {sizeof(header), sizeof(cells), sizeof(zero),
                            sizeof(faces), sizeof(zero)};
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool okay = true;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned i = 0; okay && i < sizeof(parts) / sizeof(parts[0]); ++i) {
        hash = dc_color_hash(hash, parts[i], sizes[i]);
        okay = fwrite(parts[i], sizes[i], 1, file) == 1;
    }
    okay = okay && fwrite(&hash, sizeof(hash), 1, file) == 1;
    return fclose(file) == 0 && okay;
}

static void test_completed_time_fixture_loads_with_physical_state(void) {
    char directory[] = "build/stream_clock_fixture_XXXXXX", path[256];
    ASSERT_TRUE(mkdtemp(directory));
    snprintf(path, sizeof(path), "%s/chunk_0000000000000000_0000000000000000.bin", directory);
    ASSERT_TRUE(write_chunk_clock_fixture(path));
    dc_streamer_t *stream = dc_stream_create(directory, 98, 2);
    ASSERT_TRUE(stream);
    dc_stream_result_t result = {0};
    bool loaded = dc_stream_request_load(stream, (dc_chunk_coord_t){0, 0}, 1u) &&
                  wait_result(stream, &result);
    dc_stream_destroy(stream);
    bool preserved = result.chunk && result.chunk->cells[10].fluid_mass == DC_FLUID_FULL &&
                     result.chunk->face_velocity[10].x == 1.25f &&
                     result.time.known && result.time.seconds == 42.125;
    dc_stream_result_kind_t kind = result.kind;
    dc_stream_result_release(&result);
    ASSERT_TRUE(loaded);
    ASSERT_EQ(kind, DC_STREAM_LOADED);
    ASSERT_TRUE(preserved);
    PASS();
}

static void test_known_zero_survives_restart_and_invalid_clock_cannot_replace_it(void) {
    char directory[] = "build/stream_clock_zero_XXXXXX", path[256];
    ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *stream = dc_stream_create(directory, 98, 2);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(stream && chunk);
    chunk->cells[0].material = DC_MATERIAL_WOOD;
    const dc_stream_chunk_time_t zero_time = {.known = true};
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_save_snapshot(stream, chunk, NULL, NULL, NULL, NULL,
                                               NULL, zero_time, 1u));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);

    stream = dc_stream_create(directory, 98, 2);
    ASSERT_TRUE(stream);
    chunk->cells[0].material = DC_MATERIAL_AIR;
    const double invalid[] = {NAN, INFINITY, -1.0};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        dc_stream_chunk_time_t bad = {.seconds = invalid[i], .known = true};
        ASSERT_TRUE(!dc_stream_request_save_snapshot(stream, chunk, NULL, NULL, NULL,
                                                     NULL, NULL, bad, 2u));
    }
    ASSERT_TRUE(dc_stream_request_load(stream, chunk->coord, 3u) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_TRUE(result.time.known && result.time.seconds == 0.0);
    ASSERT_EQ(result.chunk->cells[0].material, DC_MATERIAL_WOOD);
    dc_stream_result_release(&result);

    snprintf(path, sizeof(path), "%s/chunk_0000000000000000_0000000000000000.bin", directory);
    FILE *file = fopen(path, "r+b");
    ASSERT_TRUE(file);
    const double changed = 0.5;
    ASSERT_TRUE(fseek(file, offsetof(timed_chunk_fixture_header_t, completed_seconds), SEEK_SET) == 0 &&
                fwrite(&changed, sizeof(changed), 1, file) == 1);
    ASSERT_EQ(fclose(file), 0);
    ASSERT_TRUE(dc_stream_request_load(stream, chunk->coord, 4u) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_FAILED);
    ASSERT_TRUE(!result.chunk && !result.time.known);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    free(chunk);
    PASS();
}

static void test_color_chunk_roundtrip_and_corruption_rejection(void) {
    char directory[]="build/stream_color_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *stream=dc_stream_create(directory,314,2);ASSERT_TRUE(stream);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(chunk&&colors);
    chunk->coord=(dc_chunk_coord_t){-1,2};chunk->cells[0].material=DC_MATERIAL_WOOD;
    chunk->cells[4095].fluid_mass=32768;
    float terrain[4]={1.4f,.24f,.9f,1.f},water[4]={.12f,.83f,.1f,1.f};
    memcpy(colors->terrain[0],terrain,sizeof(terrain));memcpy(colors->water[4095],water,sizeof(water));
    ASSERT_TRUE(dc_stream_request_save_colors(stream,chunk,colors,9));
    colors->terrain[0][0]=8.f;
    dc_stream_result_t result={0};ASSERT_TRUE(wait_result(stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);dc_stream_destroy(stream);
    stream=dc_stream_create(directory,314,2);ASSERT_TRUE(stream);
    ASSERT_TRUE(dc_stream_request_load(stream,chunk->coord,10));ASSERT_TRUE(wait_result(stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.colors);
    ASSERT_TRUE(!memcmp(result.colors->terrain[0],terrain,sizeof(terrain)));
    ASSERT_TRUE(!memcmp(result.colors->water[4095],water,sizeof(water)));
    ASSERT_EQ(result.chunk->cells[0].material,DC_MATERIAL_WOOD);
    dc_stream_result_release(&result);
    colors->terrain[0][0]=NAN;
    ASSERT_TRUE(!dc_stream_request_save_colors(stream,chunk,colors,11));
    char path[256];snprintf(path,sizeof(path),"%s/chunk_ffffffffffffffff_0000000000000002.bin",directory);
    FILE *file=fopen(path,"r+b");ASSERT_TRUE(file);
    ASSERT_EQ(fseek(file,-1,SEEK_END),0);int byte=fgetc(file);ASSERT_TRUE(byte!=EOF);
    ASSERT_EQ(fseek(file,-1,SEEK_END),0);ASSERT_TRUE(fputc(byte^128,file)!=EOF);fclose(file);
    ASSERT_TRUE(dc_stream_request_load(stream,chunk->coord,12));ASSERT_TRUE(wait_result(stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_FAILED);ASSERT_TRUE(!result.chunk);
    dc_stream_result_release(&result);dc_stream_destroy(stream);free(chunk);free(colors);PASS();
}
static void test_bulk_liquid_chunk_roundtrip_keeps_water_and_distinct_oil_slime(void) {
    char directory[]="build/stream_bulk_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *stream=dc_stream_create(directory,314,2);ASSERT_TRUE(stream);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk));ASSERT_TRUE(chunk&&bulk);
    chunk->coord=(dc_chunk_coord_t){2,-3};chunk->cells[100].fluid_mass=30000;
    bulk->oil_fp[100]=20000;bulk->slime_fp[100]=15536;
    bulk->oil_retained_fp[100]=1234;bulk->slime_retained_fp[100]=4321;
    bulk->oil_excess[100]=17.5f;bulk->slime_excess[100]=-3.25f;
    ASSERT_TRUE(dc_stream_request_save_bulk(stream,chunk,NULL,NULL,NULL,NULL,bulk,7));
    dc_stream_result_t result={0};ASSERT_TRUE(wait_result(stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    bulk->oil_fp[100]=20001;
    ASSERT_TRUE(!dc_stream_request_save_bulk(stream,chunk,NULL,NULL,NULL,NULL,bulk,9));
    ASSERT_TRUE(dc_stream_request_load(stream,chunk->coord,8)&&wait_result(stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.bulk);
    ASSERT_INT_EQ(result.chunk->cells[100].fluid_mass,30000);
    ASSERT_INT_EQ(result.bulk->oil_fp[100],20000);ASSERT_INT_EQ(result.bulk->slime_fp[100],15536);
    ASSERT_INT_EQ(result.bulk->oil_retained_fp[100],1234);
    ASSERT_INT_EQ(result.bulk->slime_retained_fp[100],4321);
    ASSERT_EQ(result.bulk->oil_excess[100],17.5f);
    ASSERT_EQ(result.bulk->slime_excess[100],-3.25f);
    dc_stream_result_release(&result);dc_stream_destroy(stream);free(chunk);free(bulk);PASS();
}
static void test_staged_chunk_reload_prefers_the_current_world_edit(void) {
    char directory[] = "build/staged_stream_XXXXXX", err[128] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    dc_streamer_t *stream = dc_stream_create(directory, 314, 2);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    dc_stream_result_t result = {0};
    ASSERT_TRUE(save && stream && chunk);
    dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
    ASSERT_TRUE(colors);
    colors->terrain[0][0] = .5f;
    colors->terrain[0][3] = 1.0f;
    const dc_stream_chunk_time_t canonical = {.seconds = 10.125, .known = true};
    const dc_stream_chunk_time_t staged = {.seconds = 11.25, .known = true};
    chunk->coord = (dc_chunk_coord_t){-1, 0};
    chunk->cells[0].material = DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_stream_request_save_snapshot(stream, chunk, colors, NULL, NULL, NULL,
                                               NULL, canonical, 1u) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(dc_stream_stage(stream, dc_world_save_directory(save, true)));
    ASSERT_TRUE(dc_stream_request_load(stream, chunk->coord, 2) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[0].material, DC_MATERIAL_WOOD);
    ASSERT_TRUE(result.time.known && result.time.seconds == canonical.seconds);
    ASSERT_EQ(result.colors->terrain[0][0], .5f);
    dc_stream_result_release(&result);

    chunk->cells[0].material = DC_MATERIAL_AIR;
    ASSERT_TRUE(dc_stream_request_save_snapshot(stream, chunk, NULL, NULL, NULL, NULL,
                                               NULL, staged, 3u) &&
                dc_stream_request_load(stream, chunk->coord, 4u));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[0].material, DC_MATERIAL_AIR);
    ASSERT_TRUE(result.time.known && result.time.seconds == staged.seconds);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    dc_world_save_close(save);

    stream = dc_stream_create(directory, 314, 2);
    ASSERT_TRUE(stream && dc_stream_request_load(stream, chunk->coord, 5) && wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[0].material, DC_MATERIAL_WOOD);
    ASSERT_TRUE(result.time.known && result.time.seconds == canonical.seconds);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    free(colors);
    free(chunk);
    PASS();
}

int main(void) {
    RUN(test_worker_generates_saves_and_reloads_chunk);
    RUN(test_shutdown_flushes_queued_saves);
    RUN(test_resident_slot_round_trip_through_worker);
    RUN(test_worker_loads_procedural_basin);
    RUN(test_legacy_chunk_loads_with_zero_velocity);
    RUN(test_version_four_grain_radius_migrates);
    RUN(test_completed_time_fixture_loads_with_physical_state);
    RUN(test_known_zero_survives_restart_and_invalid_clock_cannot_replace_it);
    RUN(test_color_chunk_roundtrip_and_corruption_rejection);
    RUN(test_bulk_liquid_chunk_roundtrip_keeps_water_and_distinct_oil_slime);
    RUN(test_staged_chunk_reload_prefers_the_current_world_edit);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
