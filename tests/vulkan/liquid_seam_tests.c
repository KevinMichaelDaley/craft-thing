#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/liquid_seam.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"

static int g_pass, g_fail;
static char g_err[256];
static bool g_lose_receipt;

bool __real_fr_renderer_submit(dc_gpu_t *gpu);
bool __wrap_fr_renderer_submit(dc_gpu_t *gpu) {
    bool complete = __real_fr_renderer_submit(gpu);
    if (!g_lose_receipt) return complete;
    g_lose_receipt = false;
    return false;
}

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(test) do { if (!(test)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #test, g_err); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

typedef struct {
    dc_liquid_seam_endpoint_t first, second;
    dc_chunk_t *a, *b;
    fr_bulk_phase_chunk_t *bulk_a, *bulk_b;
} seam_fixture_t;

static bool init_fixture(seam_fixture_t *f, dc_liquid_seam_axis_t axis) {
    static const char shader[] = "build/shaders/pattern.comp.spv";
    g_err[0] = '\0';
    f->a = calloc(1, sizeof(*f->a));
    f->b = calloc(1, sizeof(*f->b));
    f->bulk_a = calloc(1, sizeof(*f->bulk_a));
    f->bulk_b = calloc(1, sizeof(*f->bulk_b));
    if (!f->a || !f->b || !f->bulk_a || !f->bulk_b) return false;
    f->first.tile_x = 1u;
    f->second.tile_y = 1u;
    f->second.slot = 2u;
    const dc_chunk_coord_t origin = {3, -5};
    f->a->coord = (dc_chunk_coord_t){4, -5};
    f->b->coord = (dc_chunk_coord_t){4 + (axis == DC_LIQUID_SEAM_X), -5 + (axis == DC_LIQUID_SEAM_Y)};
    f->first.coord = f->a->coord;
    f->second.coord = f->b->coord;
    const dc_chunk_coord_t other_origin = {f->b->coord.x, f->b->coord.y - 1};
    return dc_gpu_create(&f->first.gpu, 128, 64, shader, g_err, sizeof(g_err)) &&
        dc_gpu_create_shared(&f->second.gpu, f->first.gpu, 64, 192, shader, g_err, sizeof(g_err)) &&
        dc_gpu_set_body_origin(f->first.gpu, origin, g_err, sizeof(g_err)) &&
        dc_gpu_set_body_origin(f->second.gpu, other_origin, g_err, sizeof(g_err));
}

static bool seed_endpoint(dc_liquid_seam_endpoint_t ep, const dc_chunk_t *chunk,
                          const fr_bulk_phase_chunk_t *bulk) {
    return dc_gpu_upload_chunk(ep.gpu, ep.slot, chunk, g_err, sizeof(g_err)) &&
        fr_bulk_phase_gpu_import_chunk(ep.gpu, ep.slot, chunk, bulk, g_err, sizeof(g_err)) &&
        dc_gpu_set_page(ep.gpu, ep.tile_x, ep.tile_y, ep.slot, g_err, sizeof(g_err));
}

static bool download_endpoint(dc_liquid_seam_endpoint_t ep, dc_chunk_t *chunk,
                              fr_bulk_phase_chunk_t *bulk) {
    return dc_gpu_download_chunk(ep.gpu, ep.slot, chunk, g_err, sizeof(g_err)) &&
        fr_bulk_phase_gpu_export_chunk(ep.gpu, ep.slot, bulk, g_err, sizeof(g_err));
}

static void destroy_fixture(seam_fixture_t *f) {
    dc_gpu_destroy(f->second.gpu);
    dc_gpu_destroy(f->first.gpu);
    free(f->bulk_b);
    free(f->bulk_a);
    free(f->b);
    free(f->a);
}

static void test_real_seam_conserves_stock_heat_momentum_and_water_color(void) {
    seam_fixture_t f = {0};
    ASSERT_TRUE(init_fixture(&f, DC_LIQUID_SEAM_X));
    const uint32_t lane = 23u, a = lane * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u;
    const uint32_t b = lane * DC_CHUNK_SIDE;
    f.a->cells[a].fluid_mass = 18000u;
    f.b->cells[b].fluid_mass = 3000u;
    f.bulk_a->oil_fp[a] = 22000u;
    f.bulk_a->oil_retained_fp[a] = 2000u;
    f.bulk_a->oil_excess[a] = 22.f;
    f.bulk_a->oil_momentum_x[a] = 44.f;
    f.bulk_a->oil_momentum_y[a] = -11.f;
    f.bulk_b->oil_fp[b] = 2000u;
    f.bulk_b->oil_excess[b] = -1.f;
    f.bulk_a->slime_fp[a] = 8000u;
    f.bulk_b->slime_fp[b] = 12000u;
    f.bulk_b->slime_retained_fp[b] = 1000u;
    f.bulk_b->slime_excess[b] = -12.f;
    f.bulk_b->slime_momentum_x[b] = -24.f;
    f.bulk_b->slime_momentum_y[b] = 12.f;
    ASSERT_TRUE(seed_endpoint(f.first, f.a, f.bulk_a));
    ASSERT_TRUE(seed_endpoint(f.second, f.b, f.bulk_b));
    dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
    float *heat = calloc(DC_CHUNK_CELLS, sizeof(*heat));
    ASSERT_TRUE(colors && heat);
    colors->water[a][0] = colors->water[a][3] = 1.f;
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(f.first.gpu, f.first.slot, colors, g_err, sizeof(g_err)));
    colors->water[a][0] = colors->water[a][3] = 0.f;
    colors->water[b][1] = colors->water[b][3] = 1.f;
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(f.second.gpu, f.second.slot, colors, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(f.first.gpu, g_err, sizeof(g_err)) &&
        fr_thermal_gpu_enable(f.second.gpu, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_seed_water(f.first.gpu, f.first.slot, a, 9.f, g_err, sizeof(g_err)) &&
        fr_thermal_gpu_seed_water(f.second.gpu, f.second.slot, b, 1.f, g_err, sizeof(g_err)));

    dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE] = {0};
    flux[lane] = (dc_liquid_seam_flux_t){6000, 4000, -5000};
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(download_endpoint(f.first, f.a, f.bulk_a) && download_endpoint(f.second, f.b, f.bulk_b));
    ASSERT_EQ(f.a->cells[a].fluid_mass, 12000u);
    ASSERT_EQ(f.b->cells[b].fluid_mass, 9000u);
    ASSERT_EQ(f.bulk_a->oil_fp[a], 18000u);
    ASSERT_EQ(f.bulk_b->oil_fp[b], 6000u);
    ASSERT_EQ(f.bulk_a->slime_fp[a], 13000u);
    ASSERT_EQ(f.bulk_b->slime_fp[b], 7000u);
    ASSERT_EQ(f.bulk_a->oil_retained_fp[a], 2000u);
    ASSERT_EQ(f.bulk_b->slime_retained_fp[b], 1000u);
    ASSERT_TRUE(fabsf(f.bulk_a->oil_excess[a] - 18.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->oil_excess[b] - 3.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_a->oil_momentum_x[a] - 36.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->oil_momentum_x[b] - 8.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_a->oil_momentum_y[a] + 9.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->oil_momentum_y[b] + 2.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_a->slime_excess[a] + 5.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->slime_excess[b] + 7.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_a->slime_momentum_x[a] + 10.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->slime_momentum_x[b] + 14.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_a->slime_momentum_y[a] - 5.f) < 1e-5f);
    ASSERT_TRUE(fabsf(f.bulk_b->slime_momentum_y[b] - 7.f) < 1e-5f);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(f.first.gpu, f.first.slot, heat, DC_CHUNK_CELLS,
        g_err, sizeof(g_err)) && fabsf(heat[a] - 6.f) < 1e-5f);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(f.second.gpu, f.second.slot, heat, DC_CHUNK_CELLS,
        g_err, sizeof(g_err)) && fabsf(heat[b] - 4.f) < 1e-5f);
    ASSERT_TRUE(fr_renderer_download_chunk_colors(f.second.gpu, f.second.slot, colors, g_err, sizeof(g_err)));
    ASSERT_TRUE(fabsf(colors->water[b][0] - 2.f/3.f) < 1e-5f);
    ASSERT_TRUE(fabsf(colors->water[b][1] - 1.f/3.f) < 1e-5f);
    ASSERT_EQ(dc_gpu_validation_errors(f.first.gpu), 0u);
    free(heat);
    free(colors);
    destroy_fixture(&f);
    PASS();
}

static void test_one_invalid_lane_and_one_stale_binding_reject_the_whole_face(void) {
    seam_fixture_t f = {0};
    ASSERT_TRUE(init_fixture(&f, DC_LIQUID_SEAM_X));
    const uint32_t lane = 17u, blocked = 33u, full = 34u;
    const uint32_t a = lane * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u, b = lane * DC_CHUNK_SIDE;
    f.a->cells[a].fluid_mass = 16000u;
    f.a->cells[blocked * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u].fluid_mass = 7000u;
    f.b->cells[blocked * DC_CHUNK_SIDE].material = DC_MATERIAL_STONE;
    f.a->cells[full * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u].fluid_mass = 7000u;
    f.bulk_b->slime_fp[full * DC_CHUNK_SIDE] = DC_FLUID_FULL;
    f.bulk_a->oil_fp[a] = 12000u;
    f.bulk_a->oil_retained_fp[a] = 11000u;
    ASSERT_TRUE(seed_endpoint(f.first, f.a, f.bulk_a) && seed_endpoint(f.second, f.b, f.bulk_b));
    dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE] = {0};
    flux[lane].water = 5000;
    flux[blocked].water = 3000;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_REJECTED);
    flux[blocked].water = 0;
    flux[full].water = 3000;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_REJECTED);
    flux[full].water = 0;
    flux[lane].oil = 2500;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_REJECTED);
    flux[lane].oil = 0;
    ASSERT_TRUE(dc_gpu_set_page(f.second.gpu, f.second.tile_x, f.second.tile_y, 1u, g_err, sizeof(g_err)));
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(download_endpoint(f.first, f.a, f.bulk_a) && download_endpoint(f.second, f.b, f.bulk_b));
    ASSERT_EQ(f.a->cells[a].fluid_mass, 16000u);
    ASSERT_EQ(f.b->cells[b].fluid_mass, 0u);
    ASSERT_EQ(f.bulk_a->oil_fp[a], 12000u);
    ASSERT_TRUE(dc_gpu_set_page(f.second.gpu, f.second.tile_x, f.second.tile_y, f.second.slot,
        g_err, sizeof(g_err)));
    dc_liquid_seam_endpoint_t stale = f.second;
    --stale.coord.y;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, stale, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(download_endpoint(f.first, f.a, f.bulk_a) && download_endpoint(f.second, f.b, f.bulk_b));
    ASSERT_EQ(f.a->cells[a].fluid_mass, 11000u);
    ASSERT_EQ(f.b->cells[b].fluid_mass, 5000u);
    ASSERT_EQ(dc_gpu_validation_errors(f.first.gpu), 0u);
    destroy_fixture(&f);
    PASS();
}

static void test_vertical_face_accepts_reverse_flow_and_simultaneous_full_cell_swaps(void) {
    seam_fixture_t f = {0};
    ASSERT_TRUE(init_fixture(&f, DC_LIQUID_SEAM_Y));
    const uint32_t lane = 19u, a = DC_CHUNK_CELLS - DC_CHUNK_SIDE + lane, b = lane;
    f.a->cells[a].fluid_mass = 8192u;
    f.b->cells[b].fluid_mass = 24576u;
    f.bulk_a->oil_fp[a] = DC_FLUID_FULL - 8192u;
    f.bulk_b->oil_fp[b] = DC_FLUID_FULL - 24576u - 8192u;
    f.bulk_b->slime_fp[b] = 8192u;
    f.a->cells[a + 1u].fluid_mass = f.b->cells[b].fluid_mass;
    f.b->cells[b + 1u].fluid_mass = f.a->cells[a].fluid_mass;
    f.bulk_a->oil_fp[a + 1u] = f.bulk_b->oil_fp[b];
    f.bulk_b->oil_fp[b + 1u] = f.bulk_a->oil_fp[a];
    f.bulk_a->slime_fp[a + 1u] = f.bulk_b->slime_fp[b];
    ASSERT_TRUE(seed_endpoint(f.first, f.a, f.bulk_a) && seed_endpoint(f.second, f.b, f.bulk_b));
    dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE] = {0};
    flux[lane] = (dc_liquid_seam_flux_t){-8192, 12288, -4096};
    flux[lane + 1u] = (dc_liquid_seam_flux_t){8192, -12288, 4096};
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_Y, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(download_endpoint(f.first, f.a, f.bulk_a) && download_endpoint(f.second, f.b, f.bulk_b));
    for (uint32_t i = 0; i < 2u; ++i) {
        ASSERT_EQ(f.a->cells[a + i].fluid_mass, 16384u);
        ASSERT_EQ(f.b->cells[b + i].fluid_mass, 16384u);
        ASSERT_EQ(f.bulk_a->oil_fp[a + i], 45056u);
        ASSERT_EQ(f.bulk_b->oil_fp[b + i], 45056u);
        ASSERT_EQ(f.bulk_a->slime_fp[a + i], 4096u);
        ASSERT_EQ(f.bulk_b->slime_fp[b + i], 4096u);
    }
    ASSERT_EQ(dc_gpu_validation_errors(f.first.gpu), 0u);
    destroy_fixture(&f);
    PASS();
}

static void test_lost_submission_receipt_is_distinct_from_a_rejected_plan(void) {
    seam_fixture_t f = {0};
    ASSERT_TRUE(init_fixture(&f, DC_LIQUID_SEAM_X));
    const uint32_t lane = 29u, from = lane * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u;
    const uint32_t to = lane * DC_CHUNK_SIDE;
    f.a->cells[from].fluid_mass = 12000u;
    ASSERT_TRUE(seed_endpoint(f.first, f.a, f.bulk_a) && seed_endpoint(f.second, f.b, f.bulk_b));
    dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE] = {0};
    flux[lane].water = 5000;
    g_lose_receipt = true;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(f.first, f.second, DC_LIQUID_SEAM_X, flux,
        g_err, sizeof(g_err)), DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_TRUE(!g_lose_receipt);
    ASSERT_TRUE(download_endpoint(f.first, f.a, f.bulk_a) && download_endpoint(f.second, f.b, f.bulk_b));
    ASSERT_EQ(f.a->cells[from].fluid_mass, 7000u);
    ASSERT_EQ(f.b->cells[to].fluid_mass, 5000u);
    ASSERT_EQ(dc_gpu_validation_errors(f.first.gpu), 0u);
    destroy_fixture(&f);
    PASS();
}

int main(void) {
    RUN(test_real_seam_conserves_stock_heat_momentum_and_water_color);
    RUN(test_one_invalid_lane_and_one_stale_binding_reject_the_whole_face);
    RUN(test_vertical_face_accepts_reverse_flow_and_simultaneous_full_cell_swaps);
    RUN(test_lost_submission_receipt_is_distinct_from_a_rejected_plan);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
