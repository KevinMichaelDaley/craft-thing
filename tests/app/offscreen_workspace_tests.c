#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/offscreen_internal.h"

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
    if (okay && g_ticks == g_fault_tick) {
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

static bool seed_world_channels(dc_gpu_t *parent, const dc_chunk_t *chunk,
                               unsigned center, unsigned wood, fr_gas_checkpoint_chunk_t *gas,
                               fr_reactive_chunk_t *reactive, float *water) {
    const fr_gas_model_t model = {
        .gas_constant = {.25984, .29680, .18892, .46152, 0.},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.
    };
    fr_reactive_state_t parcel;
    return dc_gpu_set_body_origin(parent, chunk->coord, g_err, sizeof(g_err)) &&
           dc_gpu_upload_chunk(parent, 0u, chunk, g_err, sizeof(g_err)) &&
           dc_gpu_set_page(parent, 0u, 0u, 0u, g_err, sizeof(g_err)) &&
           fr_thermal_gpu_enable(parent, g_err, sizeof(g_err)) &&
           fr_thermal_gpu_seed_water(parent, 0u, center, .075f, g_err, sizeof(g_err)) &&
           fr_thermal_gpu_read_water_chunk(parent, 0u, water, DC_CHUNK_CELLS, g_err, sizeof(g_err)) &&
           fr_reactive_state_default(DC_MATERIAL_WOOD, 39322u, 1200u, 330.f, 30u,
                                     &parcel, g_err, sizeof(g_err)) &&
           fr_gpu_reactive_seed_cell(parent, 0u, wood, &parcel, g_err, sizeof(g_err)) &&
           fr_gpu_reactive_export_chunk(parent, 0u, chunk, reactive, g_err, sizeof(g_err)) &&
           fr_gas_gpu_enable(parent, &model, .0012f, 293.15f, g_err, sizeof(g_err)) &&
           fr_gas_gpu_export_checkpoint(parent, 0u, gas, g_err, sizeof(g_err));
}

static double gas_stock(const fr_gas_checkpoint_chunk_t *gas, unsigned species) {
    double stock = 0.;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
        stock += gas->cells[i][species];
        if (species == FR_GAS_EXHAUST) stock += gas->carrier_tail[i][0];
        if (species == FR_GAS_SOOT) stock += gas->carrier_tail[i][1];
    }
    return stock;
}

static void test_isolated_tile_advances_and_returns_through_cold_storage(void) {
    reset_ticks();
    dc_gpu_t *parent = make_parent();
    const dc_chunk_coord_t base = {-9, 4}, coord = {-7, 5};
    ASSERT_TRUE(parent);

    dc_chunk_t chunk = {.coord = coord};
    dc_chunk_colors_t colors = {0};
    fr_bulk_phase_chunk_t bulk = {0};
    const unsigned center = 32u * DC_CHUNK_SIDE + 32u;
    const unsigned wood = 8u * DC_CHUNK_SIDE + 8u;
    for (unsigned y = 31; y <= 33; ++y)
        for (unsigned x = 31; x <= 33; ++x)
            chunk.cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    chunk.cells[center] = (dc_cell_t){.fluid_mass = DC_FLUID_FULL / 2u};
    chunk.cells[wood].material = DC_MATERIAL_WOOD;
    bulk.oil_fp[center] = 4096u;
    colors.terrain[center - 1u][0] = .25f;
    colors.terrain[center - 1u][3] = 1.f;
    fr_gas_checkpoint_chunk_t gas;
    fr_reactive_chunk_t reactive = {0};
    float water_excess[DC_CHUNK_CELLS];
    ASSERT_TRUE(seed_world_channels(parent, &chunk, center, wood, &gas, &reactive, water_excess));
    dc_offscreen_t *workspace = dc_offscreen_create(parent, base, g_err, sizeof(g_err));
    ASSERT_TRUE(workspace);
    const uint64_t generation = 21;
    const dc_stream_result_t source = {.kind = DC_STREAM_LOADED, .coord = coord,
        .generation = generation, .chunk = &chunk, .colors = &colors, .bulk = &bulk,
        .gas = &gas, .reactive = &reactive, .water_excess = water_excess,
        .time = {.seconds = 14., .known = true}};
    ASSERT_TRUE(dc_offscreen_admit(workspace, &source, generation, g_err, sizeof(g_err)));
    dc_stream_result_t admitted = {0};
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, generation, &admitted, g_err, sizeof(g_err)));
    ASSERT_TRUE(admitted.gas && admitted.reactive && admitted.water_excess);
    ASSERT_EQ(memcmp(admitted.gas, &gas, sizeof(gas)), 0);
    ASSERT_EQ(memcmp(admitted.water_excess, water_excess, sizeof(water_excess)), 0);
    ASSERT_TRUE(reactive.count == 1u && admitted.reactive->count == 1u);
    const uint32_t parcel_id = reactive.attachments[0].record.state.parcel_id_low;
    ASSERT_EQ(admitted.reactive->attachments[0].record.state.parcel_id_low, parcel_id);
    dc_stream_result_release(&admitted);
    fr_reactive_chunk_release(&reactive);
    const double elapsed = .09375;
    double target = source.time.seconds + elapsed;
    ASSERT_TRUE(dc_offscreen_advance(workspace, target, 16u, g_err, sizeof(g_err)));
    ASSERT_TRUE(g_ticks > 1u && g_longest_step <= .05f && fabs(g_duration - elapsed) < 1e-13);

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
    ASSERT_TRUE(saved.chunk && saved.bulk && saved.colors && saved.water_excess && saved.gas && saved.reactive);
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
        water += saved.chunk->cells[i].fluid_mass;
        oil += saved.bulk->oil_fp[i] + saved.bulk->oil_retained_fp[i];
    }
    ASSERT_EQ(water, DC_FLUID_FULL / 2u);
    ASSERT_EQ(oil, 4096u);
    ASSERT_EQ(saved.colors->terrain[center - 1u][0], .25f);
    ASSERT_TRUE(saved.water_excess[center] > 0.f);
    ASSERT_TRUE(fabs(saved.gas->clock.advanced_seconds - elapsed) < 1e-7);
    double gas_drift = 0.;
    for (unsigned species = 0; species < FR_GAS_SPECIES; ++species)
        gas_drift = fmax(gas_drift, fabs(gas_stock(saved.gas, species) - gas_stock(&gas, species)));
    printf("Cold tile %.5f s in %u steps, maximum species drift %.9g\n", elapsed, g_ticks, gas_drift);
    ASSERT_TRUE(gas_drift < 1e-6);
    bool retained_parcel = false;
    for (uint32_t i = 0; i < saved.reactive->count; ++i) {
        const fr_reactive_attachment_t *row = &saved.reactive->attachments[i];
        retained_parcel |= row->local_index == wood && row->record.state.parcel_id_low == parcel_id;
    }
    ASSERT_TRUE(retained_parcel);
    dc_stream_result_release(&saved);
    dc_stream_destroy(stream);
    PASS();
}

static void test_absolute_target_budget_and_failed_owner_cannot_publish(void) {
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
    ASSERT_EQ(strcmp(g_err, "Injected failure after submission"), 0);
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

static void test_partial_water_cycle_holds_the_workspace_until_both_owners_finish(void) {
    reset_ticks();
    dc_gpu_t *resident = make_parent();
    dc_chunk_t chunk = {0};
    ASSERT_TRUE(dc_gpu_set_fluid_interval(resident, 2u));
    ASSERT_TRUE(dc_gpu_set_marker_correction(resident, false));
    dc_offscreen_t *workspace = dc_offscreen_create(resident, (dc_chunk_coord_t){1, 0}, g_err, sizeof(g_err));
    ASSERT_TRUE(resident && workspace);
    ASSERT_TRUE(dc_gpu_upload_chunk(resident, 0u, &chunk, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_page(resident, 0u, 0u, 0u, g_err, sizeof(g_err)));
    chunk.coord = workspace->origin;
    dc_stream_result_t source = {.kind = DC_STREAM_LOADED, .coord = chunk.coord, .chunk = &chunk,
        .generation = 1u, .time = {.known = true}};
    ASSERT_TRUE(dc_offscreen_admit(workspace, &source, 1u, g_err, sizeof(g_err)));
    dc_level_view_t view = {0};
    dc_level_offscreen_handoff_t *handoff = calloc(1, sizeof(*handoff));
    ASSERT_TRUE(handoff);
    *handoff = (dc_level_offscreen_handoff_t){.workspace = workspace, .coord = chunk.coord,
        .generation = 1u, .admitted = true};
    view.offscreen_handoffs = handoff;
    const dc_liquid_seam_endpoint_t tiles[] = {{.gpu = resident}, {.gpu = workspace->gpu, .coord = chunk.coord}};
    ASSERT_EQ(dc_gpu_step_water_cohort(tiles, 2u, 1.f / 60.f, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    dc_stream_result_t snapshot = {0};
    ASSERT_TRUE(!dc_offscreen_snapshot(workspace, chunk.coord, 1u, &snapshot, g_err, sizeof(g_err)));
    ASSERT_TRUE(!snapshot.chunk);
    ASSERT_TRUE(!dc_gpu_set_marker_correction(resident, true));
    ASSERT_TRUE(dc_gpu_set_marker_correction(workspace->gpu, false));
    ASSERT_TRUE(!dc_level_offscreen_cancel(&view, chunk.coord, 1u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_level_offscreen_owns(&view, chunk.coord));
    ASSERT_TRUE(dc_offscreen_advance(workspace, .125, 1u, g_err, sizeof(g_err)));
    ASSERT_EQ(g_ticks, 0u);
    ASSERT_EQ(workspace->time.seconds, 0.);
    dc_chunk_t neighbor = {.coord = {2, 0}};
    dc_stream_result_t next = source;
    next.coord = neighbor.coord;
    next.chunk = &neighbor;
    next.generation = 2u;
    ASSERT_TRUE(!dc_offscreen_admit(workspace, &next, 2u, g_err, sizeof(g_err)));
    ASSERT_EQ(dc_gpu_step_water_cohort(tiles, 2u, 1.f / 60.f, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(dc_offscreen_admit(workspace, &next, 2u, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, chunk.coord, 1u, &snapshot, g_err, sizeof(g_err)));
    dc_stream_result_release(&snapshot);
    ASSERT_TRUE(dc_level_offscreen_cancel(&view, chunk.coord, 1u, g_err, sizeof(g_err)));
    ASSERT_TRUE(!dc_level_offscreen_owns(&view, chunk.coord));
    dc_offscreen_destroy(workspace);
    dc_gpu_destroy(resident);
    PASS();
}

int main(void) {
    RUN(test_partial_water_cycle_holds_the_workspace_until_both_owners_finish);
    RUN(test_isolated_tile_advances_and_returns_through_cold_storage);
    RUN(test_absolute_target_budget_and_failed_owner_cannot_publish);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
