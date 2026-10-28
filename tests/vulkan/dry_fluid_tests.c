#include <stdio.h>
#include <stdlib.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static unsigned g_fluid_phases;
static char g_error[256];
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s (%s)\n", __LINE__, #e, g_error); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

void __real_dc_gpu_record_fluid_phase(dc_gpu_t *, uint32_t);
void __wrap_dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase) {
    g_fluid_phases++;
    __real_dc_gpu_record_fluid_phase(gpu, phase);
}

static dc_gpu_t *context(void) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = chunk && dc_gpu_create(&gpu, 64, 64,
        "build/shaders/pattern.comp.spv", g_error, sizeof(g_error));
    if (okay) okay = dc_gpu_upload_chunk(gpu, 0, chunk, g_error, sizeof(g_error)) &&
        dc_gpu_set_page(gpu, 0, 0, 0, g_error, sizeof(g_error)) &&
        dc_gpu_set_fluid_interval(gpu, 2);
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    g_fluid_phases = 0;
    return gpu;
}

static uint64_t water_mass(dc_gpu_t *gpu) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    uint64_t mass = 0;
    bool okay = chunk && dc_gpu_download_chunk(gpu, 0, chunk, g_error, sizeof(g_error));
    if (okay) for (unsigned i = 0; i < DC_CHUNK_CELLS; i++) mass += chunk->cells[i].fluid_mass;
    free(chunk);
    return okay ? mass : UINT64_MAX;
}

static void test_dry_scheduled_ticks_do_not_record_fluid_phases(void) {
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    for (unsigned i = 0; i < 4; i++) {
        ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
        ASSERT_EQ(gpu->fluid_phase, ((i + 1u) * 3u) % 6u);
        ASSERT_EQ(gpu->fluid_tick, (i + 1u) / 2u);
    }
    ASSERT_EQ(g_fluid_phases, 0u);
    ASSERT_EQ(water_mass(gpu), 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_water_upload_wakes_solver_and_preserves_mass(void) {
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    chunk->cells[8 * 64 + 8].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, g_error, sizeof(g_error)));
    free(chunk);
    for (unsigned i = 0; i < 4; i++) ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(g_fluid_phases > 0);
    ASSERT_EQ(water_mass(gpu), DC_FLUID_FULL);
    dc_gpu_destroy(gpu); PASS();
}

static void test_water_brush_wakes_solver_after_dry_ticks(void) {
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    g_fluid_phases = 0;
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 10, 10, 1, DC_MATERIAL_WATER, g_error, sizeof(g_error)));
    uint64_t before = water_mass(gpu); ASSERT_TRUE(before > 0 && before != UINT64_MAX);
    for (unsigned i = 0; i < 4; i++) ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(g_fluid_phases > 0);
    ASSERT_EQ(water_mass(gpu), before);
    dc_gpu_destroy(gpu); PASS();
}

static void test_water_source_wakes_solver_after_dry_ticks(void) {
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    g_fluid_phases = 0;
    ASSERT_TRUE(dc_gpu_set_tick_water_source(gpu, true, 8, 8));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(g_fluid_phases > 0);
    ASSERT_TRUE(water_mass(gpu) > 0);
    dc_gpu_destroy(gpu); PASS();
}

static void test_loaded_velocity_keeps_projection_active(void) {
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    chunk->face_velocity[8 * 64 + 8].x = .5f;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, g_error, sizeof(g_error)));
    free(chunk);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(g_fluid_phases > 0);
    dc_gpu_destroy(gpu); PASS();
}

static void test_complete_dry_replacement_retires_old_water_work(void) {
    dc_gpu_t *gpu=context();ASSERT_TRUE(gpu);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[8*64+8].fluid_mass=DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu,0,chunk,g_error,sizeof(g_error)));
    ASSERT_TRUE(dc_gpu_tick_step(gpu,g_error,sizeof(g_error)));
    *chunk=(dc_chunk_t){0};
    for(uint32_t slot=0;slot<DC_GPU_CHUNK_SLOTS;slot++) {
        chunk->coord=(dc_chunk_coord_t){slot,0};
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,slot,chunk,g_error,sizeof(g_error)));
    }
    ASSERT_TRUE(!gpu->fluid_work_possible);
    g_fluid_phases=0;
    ASSERT_TRUE(dc_gpu_tick_step(gpu,g_error,sizeof(g_error)));
    ASSERT_EQ(g_fluid_phases,0u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu,10,10,1,DC_MATERIAL_WATER,g_error,sizeof(g_error)));
    ASSERT_TRUE(gpu->fluid_work_possible);
    ASSERT_TRUE(dc_gpu_tick_step(gpu,g_error,sizeof(g_error)));
    ASSERT_TRUE(g_fluid_phases>0u);
    free(chunk);dc_gpu_destroy(gpu);PASS();
}

int main(void) {
    RUN(test_dry_scheduled_ticks_do_not_record_fluid_phases);
    RUN(test_water_upload_wakes_solver_and_preserves_mass);
    RUN(test_water_brush_wakes_solver_after_dry_ticks);
    RUN(test_water_source_wakes_solver_after_dry_ticks);
    RUN(test_loaded_velocity_keeps_projection_active);
    RUN(test_complete_dry_replacement_retires_old_water_work);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
