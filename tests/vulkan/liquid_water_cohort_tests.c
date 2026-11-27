#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

enum { CENTER, EAST, SOUTH, TILE_COUNT };
static const float step_seconds = 1.f / 30.f;
static int g_pass, g_fail;
static bool g_lose_ack;
static char g_err[256];

bool __real_fr_renderer_submit(dc_gpu_t *gpu);

bool __wrap_fr_renderer_submit(dc_gpu_t *gpu) {
    const bool lose = g_lose_ack;
    g_lose_ack = false;
    return __real_fr_renderer_submit(gpu) && !lose;
}

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #expr, g_err); ++g_fail; goto cleanup; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

typedef struct {
    dc_chunk_t chunk;
    dc_chunk_colors_t colors;
    float heat[DC_CHUNK_CELLS];
    uint32_t guide[DC_CHUNK_CELLS];
} water_tile_t;

static void open_corridor(water_tile_t *tile, bool vertical, bool donor) {
    const uint32_t lane = 56;
    for (uint32_t normal = donor ? 48u : 0u; normal < DC_CHUNK_SIDE; ++normal) {
        const uint32_t cell = vertical ? normal * DC_CHUNK_SIDE + lane : lane * DC_CHUNK_SIDE + normal;
        tile->chunk.cells[cell].material = DC_MATERIAL_AIR;
        if (vertical) tile->chunk.face_velocity[cell].y = 6.25f;
        else tile->chunk.face_velocity[cell].x = 7.5f;
        if (!donor || normal < 53u) continue;
        tile->chunk.cells[cell].fluid_mass = DC_FLUID_FULL / 2u;
        tile->colors.water[cell][vertical ? 2 : 0] = .85f;
        tile->colors.water[cell][3] = 1.f;
        tile->heat[cell] = vertical ? -1.5f : 2.f;
    }
}

static void seed(water_tile_t tiles[TILE_COUNT], const dc_liquid_seam_endpoint_t owners[TILE_COUNT]) {
    for (uint32_t owner = 0; owner < TILE_COUNT; ++owner) {
        tiles[owner].chunk.coord = owners[owner].coord;
        for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell)
            tiles[owner].chunk.cells[cell].material = DC_MATERIAL_STONE;
    }
    open_corridor(&tiles[CENTER], false, true);
    open_corridor(&tiles[CENTER], true, true);
    open_corridor(&tiles[EAST], false, false);
    open_corridor(&tiles[SOUTH], true, false);
    dc_chunk_t *center = &tiles[CENTER].chunk;
    center->marker_count = 1;
    center->markers[0] = (dc_marker_t){62u * DC_FLUID_FULL, 56u * DC_FLUID_FULL + DC_FLUID_FULL / 2u,
                                      1001u, DC_MARKER_INSIDE};
}

static bool upload(dc_liquid_seam_endpoint_t tile, const water_tile_t *state) {
    return dc_gpu_upload_chunk(tile.gpu, tile.slot, &state->chunk, g_err, sizeof(g_err)) &&
        dc_gpu_set_page(tile.gpu, tile.tile_x, tile.tile_y, tile.slot, g_err, sizeof(g_err)) &&
        fr_renderer_upload_chunk_colors(tile.gpu, tile.slot, &state->colors, g_err, sizeof(g_err)) &&
        fr_thermal_gpu_import_water_chunk(tile.gpu, tile.slot, &state->chunk, state->heat, g_err, sizeof(g_err));
}

static bool download(dc_liquid_seam_endpoint_t tile, water_tile_t *state) {
    if (!dc_gpu_download_chunk(tile.gpu, tile.slot, &state->chunk, g_err, sizeof(g_err)) ||
        !fr_renderer_download_chunk_colors(tile.gpu, tile.slot, &state->colors, g_err, sizeof(g_err)) ||
        !fr_thermal_gpu_read_water_chunk(tile.gpu, tile.slot, state->heat, DC_CHUNK_CELLS, g_err, sizeof(g_err)))
        return false;
    memcpy(state->guide, (const uint32_t *)tile.gpu->marker_grid_mapped + tile.slot * DC_CHUNK_CELLS,
           sizeof(state->guide));
    return true;
}

static int compare_markers(const void *a, const void *b) {
    const dc_marker_t *left = a, *right = b;
#define COMPARE(field) if (left->field != right->field) return left->field < right->field ? -1 : 1
    COMPARE(id);
    COMPARE(x_fp);
    COMPARE(y_fp);
    COMPARE(kind);
#undef COMPARE
    return 0;
}

static bool same_water(water_tile_t *a, water_tile_t *b) {
    if (a->chunk.marker_count != b->chunk.marker_count ||
        memcmp(a->chunk.cells, b->chunk.cells, sizeof(a->chunk.cells)) ||
        memcmp(a->guide, b->guide, sizeof(a->guide))) return false;
    const uint32_t markers = a->chunk.marker_count;
    qsort(a->chunk.markers, markers, sizeof(dc_marker_t), compare_markers);
    qsort(b->chunk.markers, markers, sizeof(dc_marker_t), compare_markers);
    if (memcmp(a->chunk.markers, b->chunk.markers, markers * sizeof(dc_marker_t))) return false;
    for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
        const dc_face_velocity_t va = a->chunk.face_velocity[cell], vb = b->chunk.face_velocity[cell];
        if (fabsf(va.x - vb.x) > 1e-5f || fabsf(va.y - vb.y) > 1e-5f ||
            fabsf(a->heat[cell] - b->heat[cell]) > 1e-5f) return false;
        for (uint32_t color = 0; color < 4u; ++color)
            if (fabsf(a->colors.water[cell][color] - b->colors.water[cell][color]) > 1e-5f) return false;
    }
    return true;
}

static uint32_t stock(const water_tile_t *tile) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) sum += tile->chunk.cells[i].fluid_mass;
    return sum;
}

static void test_connected_tiles_match_monolithic_water_and_share_one_receipt(void) {
    const char shader[] = "build/shaders/pattern.comp.spv";
    const dc_chunk_coord_t origin = {-7, -3};
    dc_gpu_t device = {0}, *resident = NULL, *offscreen = NULL, *mono = NULL;
    water_tile_t *initial = calloc(TILE_COUNT, sizeof(*initial));
    water_tile_t *before = calloc(TILE_COUNT, sizeof(*before));
    water_tile_t *published = calloc(TILE_COUNT, sizeof(*published));
    water_tile_t *observed = calloc(1, sizeof(*observed));
    dc_liquid_seam_endpoint_t tiles[TILE_COUNT] = {
        [CENTER] = {.coord = origin, .tile_x = 1, .tile_y = 1, .slot = 2},
        [EAST] = {.coord = {origin.x + 1, origin.y}, .tile_x = 1, .slot = 3},
        [SOUTH] = {.coord = {origin.x, origin.y + 1}, .tile_y = 1, .slot = 1}
    };
    ASSERT_TRUE(initial && before && published && observed);
    seed(initial, tiles);
    ASSERT_TRUE(dc_gpu_initialize_device(&device, 4u * DC_CHUNK_CELLS * sizeof(dc_cell_t),
                                        0, 0, 0, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&resident, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&offscreen, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(resident, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(offscreen, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(resident, (dc_chunk_coord_t){origin.x - 1, origin.y - 1},
                                      g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(offscreen, origin, g_err, sizeof(g_err)));
    tiles[CENTER].gpu = resident;
    tiles[EAST].gpu = tiles[SOUTH].gpu = offscreen;
    offscreen->marker_ping = 1u;
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) {
        ASSERT_TRUE(upload(tiles[tile], &initial[tile]));
        ASSERT_TRUE(download(tiles[tile], &before[tile]));
    }

    dc_liquid_seam_endpoint_t reordered[] = {tiles[SOUTH], tiles[EAST], tiles[CENTER]};
    reordered[1].slot = tiles[SOUTH].slot;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_REJECTED);
    reordered[1] = tiles[EAST];
    float *bad_color = (float *)offscreen->water_color_mapped +
        (tiles[SOUTH].slot * DC_CHUNK_CELLS + DC_CHUNK_CELLS - 1u) * 4u;
    const float original_color = *bad_color;
    *bad_color = NAN;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(isnan(*bad_color));
    *bad_color = original_color;
    ASSERT_TRUE(!dc_gpu_liquid_seam_pending(resident) && !dc_gpu_liquid_seam_pending(offscreen));
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) {
        ASSERT_TRUE(download(tiles[tile], observed));
        ASSERT_TRUE(same_water(observed, &before[tile]));
    }

    g_lose_ack = true;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_TRUE(dc_gpu_liquid_seam_pending(resident) && dc_gpu_liquid_seam_pending(offscreen));
    ASSERT_EQ(dc_gpu_resolve_liquid_seam(resident, offscreen, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(!dc_gpu_liquid_seam_pending(resident) && !dc_gpu_liquid_seam_pending(offscreen));
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile)
        ASSERT_TRUE(download(tiles[tile], &published[tile]));
    ASSERT_TRUE(stock(&published[EAST]) > 0 && stock(&published[SOUTH]) > 0);
    ASSERT_EQ(stock(&published[CENTER]) + stock(&published[EAST]) + stock(&published[SOUTH]),
              stock(&initial[CENTER]));
    dc_gpu_destroy(resident);
    dc_gpu_destroy(offscreen);
    resident = offscreen = NULL;

    ASSERT_TRUE(dc_gpu_create_shared(&mono, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(mono, origin, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(mono, g_err, sizeof(g_err)));
    const dc_liquid_seam_endpoint_t monolithic[] = {
        {.gpu = mono, .coord = tiles[CENTER].coord, .slot = 0},
        {.gpu = mono, .coord = tiles[EAST].coord, .tile_x = 1, .slot = 1},
        {.gpu = mono, .coord = tiles[SOUTH].coord, .tile_y = 1, .slot = 2}
    };
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) ASSERT_TRUE(upload(monolithic[tile], &initial[tile]));
    mono->timed_fluid = true;
    mono->fluid_step_scale = step_seconds * 60.f;
    ASSERT_TRUE(dc_gpu_fluid_step(mono, g_err, sizeof(g_err)));
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) {
        ASSERT_TRUE(download(monolithic[tile], observed));
        ASSERT_TRUE(same_water(&published[tile], observed));
    }
    ASSERT_INT_EQ(dc_gpu_validation_errors(&device), 0);
    PASS();

cleanup:
    g_lose_ack = false;
    dc_gpu_destroy(mono);
    dc_gpu_destroy(offscreen);
    dc_gpu_destroy(resident);
    dc_gpu_release_device(&device);
    free(observed);
    free(published);
    free(before);
    free(initial);
}

int main(void) {
    RUN(test_connected_tiles_match_monolithic_water_and_share_one_receipt);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
