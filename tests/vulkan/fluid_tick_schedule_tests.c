#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/fluid_schedule.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static char calls[80];
static uint32_t call_count;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #expr, calls); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static void record(char call) {
    if (call_count < sizeof(calls) - 1u) calls[call_count++] = call;
    calls[call_count] = '\0';
}

void __wrap_dc_gpu_record_tick_water_source(dc_gpu_t *gpu) { (void)gpu; record('s'); }
void __wrap_dc_gpu_record_rigid(dc_gpu_t *gpu) { (void)gpu; record('r'); }
void __wrap_dc_gpu_record_rigid_refresh(dc_gpu_t *gpu) { (void)gpu; record('r'); }
void __wrap_dc_gpu_record_mpm(dc_gpu_t *gpu) { (void)gpu; record('m'); }
void __wrap_dc_gpu_record_bulk_phase(dc_gpu_t *gpu, float seconds) { (void)gpu; (void)seconds; record('b'); }
void __wrap_fr_thermal_record_step(dc_gpu_t *gpu, float seconds) { (void)gpu; (void)seconds; record('h'); }
void __wrap_vkCmdPipelineBarrier2(VkCommandBuffer command, const VkDependencyInfo *dependency) {
    (void)command;
    (void)dependency;
}
void __wrap_dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase) {
    record((char)('0' + phase));
    if (phase == DC_FLUID_PHASES - 1u) ++gpu->fluid_tick;
}

static void test_tick_records_planned_phases_between_native_physics_stages(void) {
    dc_gpu_t gpu = {.fluid_work_possible = true};
    ASSERT_TRUE(dc_gpu_set_fluid_interval(&gpu, 2u));
    const float elapsed[] = {1.f / 120.f, 1.f / 60.f, 1.f / 120.f};
    const char *expected[] = {"sr0bmh", "sr123bmh", "sr45bmh"};
    const uint32_t ends[] = {1u, 4u, 0u};
    const float budgets[] = {0.5f, 0.5f, 0.f};
    for (uint32_t i = 0; i < sizeof(elapsed) / sizeof(elapsed[0]); ++i) {
        call_count = 0;
        ASSERT_TRUE(dc_gpu_set_tick_seconds(&gpu, elapsed[i]));
        dc_gpu_record_tick_step(&gpu);
        ASSERT_INT_EQ(strcmp(calls, expected[i]), 0);
        ASSERT_INT_EQ(gpu.fluid_phase, ends[i]);
        ASSERT_EQ(gpu.fluid_phase_budget, budgets[i]);
        ASSERT_EQ(gpu.fluid_step_scale, 2.f);
    }
    ASSERT_INT_EQ(gpu.fluid_tick, 1);
    PASS();
}

static void test_dry_tick_consumes_cycles_without_recording_water(void) {
    dc_gpu_t gpu = {0};
    call_count = 0;
    ASSERT_TRUE(dc_gpu_set_fluid_interval(&gpu, 2u));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(&gpu, 1.f / 15.f));
    dc_gpu_record_tick_step(&gpu);
    ASSERT_INT_EQ(strcmp(calls, "srrrrbmh"), 0);
    ASSERT_INT_EQ(gpu.fluid_tick, 2);
    ASSERT_INT_EQ(gpu.fluid_phase, 0);
    ASSERT_EQ(gpu.fluid_phase_budget, 0.f);
    PASS();
}

int main(void) {
    RUN(test_tick_records_planned_phases_between_native_physics_stages);
    RUN(test_dry_tick_consumes_cycles_without_recording_water);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
