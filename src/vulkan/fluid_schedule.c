#include "fluid_schedule.h"

dc_fluid_schedule_t dc_gpu_plan_fluid_tick(uint32_t interval, uint32_t phase, float budget,
    float tick_scale, bool timed) {
    const float elapsed = timed ? tick_scale : 1.f;
    if (interval == 1u)
        return (dc_fluid_schedule_t){
            .phase_count = DC_FLUID_PHASES, .next_phase = phase,
            .step_scale = elapsed, .remaining_budget = budget,
        };

    const float available = budget + elapsed * ((float)DC_FLUID_PHASES / (float)interval);
    const uint32_t count = (uint32_t)available;
    return (dc_fluid_schedule_t){
        .first_phase = phase, .phase_count = count, .next_phase = (phase + count) % DC_FLUID_PHASES,
        .step_scale = timed ? (float)interval : 1.f, .remaining_budget = available - (float)count,
    };
}
