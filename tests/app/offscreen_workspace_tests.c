#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "../../src/app/offscreen.h"

#ifdef DC_OFFSCREEN_RED
#pragma weak dc_offscreen_create
#pragma weak dc_offscreen_destroy
#pragma weak dc_offscreen_admit
#pragma weak dc_offscreen_advance
#pragma weak dc_offscreen_snapshot
#endif

static int g_pass, g_fail;
static char g_err[256];
static uint32_t g_ticks, g_fault_tick;
static double g_duration;
static float g_longest_step;

#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %d: %s [%s]\n", __LINE__, #expr, g_err); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

bool __real_dc_gpu_tick_step(dc_gpu_t *gpu, char *err, uint32_t cap);
bool __wrap_dc_gpu_tick_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    bool okay = __real_dc_gpu_tick_step(gpu, err, cap);
    ++g_ticks;
    if (g_ticks == g_fault_tick) {
        if (err && cap) snprintf(err, cap, "Injected failure after submission");
        return false;
    }
    return okay;
}

static void reset_ticks(void) {
    g_ticks = g_fault_tick = 0;
    g_duration = 0.;
    g_longest_step = 0.f;
    g_err[0] = '\0';
}

bool __real_dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds);
bool __wrap_dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds) {
    g_duration += seconds;
    if (seconds > g_longest_step) g_longest_step = seconds;
    return __real_dc_gpu_set_tick_seconds(gpu, seconds);
}

static bool available(void) {
#ifdef DC_OFFSCREEN_RED
    return dc_offscreen_create && dc_offscreen_admit && dc_offscreen_advance && dc_offscreen_snapshot;
#else
    return true;
#endif
}

static dc_gpu_t *make_parent(void) {
    dc_gpu_t *parent = NULL;
    dc_gpu_create(&parent, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
                  "build/shaders/pattern.comp.spv", g_err, sizeof(g_err));
    return parent;
}

static bool wait_stream(dc_streamer_t *stream, dc_stream_result_t *out) {
    uint64_t limit = SDL_GetTicks64() + 3000u;
    do {
        if (dc_stream_poll(stream, out)) return true;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < limit);
    return false;
}

static void test_isolated_tile_advances_and_returns_through_cold_storage(void) {
    ASSERT_TRUE(available());
    reset_ticks();
    dc_gpu_t *parent = make_parent();
    const dc_chunk_coord_t base = {-9, 4}, coord = {-7, 5};
    dc_offscreen_t *workspace = dc_offscreen_create(parent, base, g_err, sizeof(g_err));
    ASSERT_TRUE(parent && workspace);

    dc_chunk_t chunk = {.coord = coord};
    dc_chunk_colors_t colors = {0};
    fr_bulk_phase_chunk_t bulk = {0};
    const unsigned center = 32u * DC_CHUNK_SIDE + 32u;
    for (unsigned y = 31; y <= 33; ++y)
        for (unsigned x = 31; x <= 33; ++x)
            chunk.cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    chunk.cells[center] = (dc_cell_t){.fluid_mass = DC_FLUID_FULL / 2u};
    bulk.oil_fp[center] = 4096u;
    colors.terrain[center - 1u][0] = .25f;
    colors.terrain[center - 1u][3] = 1.f;
    const uint64_t generation = 21;
    const dc_stream_result_t source = {.kind = DC_STREAM_LOADED, .coord = coord,
        .generation = generation, .chunk = &chunk, .colors = &colors, .bulk = &bulk,
        .time = {.seconds = 14., .known = true}};
    ASSERT_TRUE(dc_offscreen_admit(workspace, &source, generation, g_err, sizeof(g_err)));
    double target = 14.09375;
    ASSERT_TRUE(dc_offscreen_advance(workspace, target, 16u, g_err, sizeof(g_err)));
    ASSERT_TRUE(g_ticks > 1u && g_longest_step <= .05f && fabs(g_duration - .09375) < 1e-13);

    dc_stream_result_t saved = {0};
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, generation, &saved, g_err, sizeof(g_err)));
    ASSERT_TRUE(saved.time.known && saved.time.seconds == target);
    dc_offscreen_destroy(workspace);
    ASSERT_EQ(dc_gpu_validation_errors(parent), 0u);
    dc_gpu_destroy(parent);

    char directory[] = "build/offscreen-return-XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *stream = dc_stream_create(directory, 51u, 2u);
    ASSERT_TRUE(stream && dc_stream_request_save_snapshot(stream, saved.chunk, saved.colors,
        saved.reactive, saved.water_excess, saved.gas, saved.bulk, saved.time, saved.generation));
    dc_stream_result_release(&saved);
    ASSERT_TRUE(wait_stream(stream, &saved) && saved.kind == DC_STREAM_SAVED);
    ASSERT_EQ(saved.generation, generation);
    dc_stream_result_release(&saved);
    ASSERT_TRUE(dc_stream_request_load(stream, coord, generation + 1u) && wait_stream(stream, &saved));
    ASSERT_TRUE(saved.kind == DC_STREAM_LOADED && saved.time.known && saved.time.seconds == target);
    ASSERT_EQ(saved.generation, generation + 1u);
    uint32_t water = 0, oil = 0;
    ASSERT_TRUE(saved.chunk && saved.bulk && saved.colors);
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
        water += saved.chunk->cells[i].fluid_mass;
        oil += saved.bulk->oil_fp[i] + saved.bulk->oil_retained_fp[i];
    }
    ASSERT_EQ(water, DC_FLUID_FULL / 2u);
    ASSERT_EQ(oil, 4096u);
    ASSERT_EQ(saved.colors->terrain[center - 1u][0], .25f);
    dc_stream_result_release(&saved);
    dc_stream_destroy(stream);
    PASS();
}

static void test_absolute_target_budget_and_failed_owner_cannot_publish(void) {
    ASSERT_TRUE(available());
    reset_ticks();
    dc_gpu_t *parent = make_parent();
    const dc_chunk_coord_t origin = {3, -4};
    dc_offscreen_t *workspace = dc_offscreen_create(parent, origin, g_err, sizeof(g_err));
    ASSERT_TRUE(parent && workspace);
    ASSERT_TRUE(dc_offscreen_advance(workspace, 3., 8u, g_err, sizeof(g_err)));
    ASSERT_EQ(g_ticks, 0u);
    dc_chunk_t chunk = {.coord = origin};
    dc_stream_result_t source = {.kind = DC_STREAM_LOADED, .coord = origin, .chunk = &chunk, .generation = 9};
    ASSERT_TRUE(!dc_offscreen_admit(workspace, &source, 9u, g_err, sizeof(g_err)));
    source.time = (dc_chunk_time_t){.seconds = 2.5, .known = true};
    ASSERT_TRUE(!dc_offscreen_admit(workspace, &source, 10u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_offscreen_admit(workspace, &source, 9u, g_err, sizeof(g_err)));
    ASSERT_TRUE(!dc_offscreen_admit(workspace, &source, 9u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, 2.625, 1u, g_err, sizeof(g_err)));
    ASSERT_EQ(g_ticks, 1u);
    dc_stream_result_t recovery = {0};
    ASSERT_TRUE(!dc_offscreen_snapshot(workspace, origin, 8u, &recovery, g_err, sizeof(g_err)));
    ASSERT_TRUE(!recovery.chunk);
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, origin, 9u, &recovery, g_err, sizeof(g_err)));
    ASSERT_TRUE(recovery.time.known && recovery.time.seconds > 2.5 && recovery.time.seconds < 2.625);

    g_fault_tick = 2u;
    ASSERT_TRUE(!dc_offscreen_advance(workspace, 2.625, 16u, g_err, sizeof(g_err)));
    ASSERT_TRUE(!dc_offscreen_advance(workspace, 2.625, 16u, g_err, sizeof(g_err)));
    dc_stream_result_t result = {0};
    ASSERT_TRUE(!dc_offscreen_snapshot(workspace, origin, 9u, &result, g_err, sizeof(g_err)));
    ASSERT_TRUE(!result.chunk);
    ASSERT_EQ(g_ticks, 2u);
    dc_offscreen_destroy(workspace);

    workspace = dc_offscreen_create(parent, origin, g_err, sizeof(g_err));
    ASSERT_TRUE(workspace && dc_offscreen_admit(workspace, &recovery, 9u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, 2.625, 16u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, origin, 9u, &result, g_err, sizeof(g_err)));
    ASSERT_TRUE(result.time.known && result.time.seconds == 2.625);
    uint32_t confirmed = g_ticks;
    ASSERT_TRUE(dc_offscreen_advance(workspace, 2.625, 16u, g_err, sizeof(g_err)));
    ASSERT_EQ(g_ticks, confirmed);
    dc_stream_result_release(&result);
    dc_stream_result_release(&recovery);
    dc_offscreen_destroy(workspace);
    ASSERT_EQ(dc_gpu_validation_errors(parent), 0u);
    dc_gpu_destroy(parent);
    PASS();
}

int main(void) {
    RUN(test_isolated_tile_advances_and_returns_through_cold_storage);
    RUN(test_absolute_target_budget_and_failed_owner_cannot_publish);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
