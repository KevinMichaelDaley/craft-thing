#include <stdio.h>

#include "../../src/vulkan/fluid_schedule.h"

static int g_pass, g_fail;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static void test_single_interval_runs_the_complete_cycle_at_elapsed_scale(void) {
    const dc_fluid_schedule_t plan = dc_gpu_plan_fluid_tick(1u, 0u, 0.f, 2.25f, true);
    ASSERT_INT_EQ(plan.first_phase, 0);
    ASSERT_INT_EQ(plan.phase_count, 6);
    ASSERT_INT_EQ(plan.next_phase, 0);
    ASSERT_EQ(plan.step_scale, 2.25f);
    ASSERT_EQ(plan.remaining_budget, 0.f);
    PASS();
}

static void test_reference_ticks_preserve_every_supported_phase_cadence(void) {
    const uint32_t intervals[] = {1u, 2u, 3u, 6u};
    for (uint32_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i) {
        const uint32_t interval = intervals[i];
        uint32_t phase = 0;
        float budget = 0.f;
        for (uint32_t step = 0; step < interval; ++step) {
            const dc_fluid_schedule_t plan = dc_gpu_plan_fluid_tick(interval, phase, budget, 1.f, true);
            ASSERT_INT_EQ(plan.first_phase, step * 6u / interval);
            ASSERT_INT_EQ(plan.phase_count, 6u / interval);
            ASSERT_INT_EQ(plan.next_phase, ((step + 1u) * 6u / interval) % 6u);
            ASSERT_EQ(plan.step_scale, (float)interval);
            ASSERT_EQ(plan.remaining_budget, 0.f);
            phase = plan.next_phase;
            budget = plan.remaining_budget;
        }
    }
    PASS();
}

static void test_mixed_steps_keep_fractional_debt_and_wrap_only_scheduled_phases(void) {
    const float elapsed[] = {0.25f, 0.5f, 0.5f, 3.0f, 9.5f};
    const uint32_t counts[] = {0, 0, 1, 3, 9};
    const uint32_t ends[] = {4, 4, 5, 2, 5};
    const float remainder[] = {0.25f, 0.75f, 0.25f, 0.25f, 0.75f};
    uint32_t phase = 4;
    float budget = 0.f;
    for (uint32_t i = 0; i < sizeof(elapsed) / sizeof(elapsed[0]); ++i) {
        const dc_fluid_schedule_t plan = dc_gpu_plan_fluid_tick(6u, phase, budget, elapsed[i], true);
        ASSERT_INT_EQ(plan.first_phase, phase);
        ASSERT_INT_EQ(plan.phase_count, counts[i]);
        ASSERT_INT_EQ(plan.next_phase, ends[i]);
        ASSERT_EQ(plan.step_scale, 6.f);
        ASSERT_EQ(plan.remaining_budget, remainder[i]);
        phase = plan.next_phase;
        budget = plan.remaining_budget;
    }
    PASS();
}

static void test_untimed_ticks_ignore_elapsed_scale_but_retain_phase_debt(void) {
    const dc_fluid_schedule_t plan = dc_gpu_plan_fluid_tick(3u, 5u, 0.5f, 10.f, false);
    ASSERT_INT_EQ(plan.first_phase, 5);
    ASSERT_INT_EQ(plan.phase_count, 2);
    ASSERT_INT_EQ(plan.next_phase, 1);
    ASSERT_EQ(plan.step_scale, 1.f);
    ASSERT_EQ(plan.remaining_budget, 0.5f);
    PASS();
}

int main(void) {
    RUN(test_single_interval_runs_the_complete_cycle_at_elapsed_scale);
    RUN(test_reference_ticks_preserve_every_supported_phase_cadence);
    RUN(test_mixed_steps_keep_fractional_debt_and_wrap_only_scheduled_phases);
    RUN(test_untimed_ticks_ignore_elapsed_scale_but_retain_phase_debt);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
