#include "fluid_schedule.h"

dc_fluid_schedule_t dc_gpu_plan_fluid_tick(uint32_t interval, uint32_t phase, float budget,
    float tick_scale, bool timed) {
    const float elapsed = timed ? tick_scale : 1.f;
    if (interval == 1u)
        return (dc_fluid_schedule_t){0u, DC_FLUID_PHASES, phase, elapsed, budget};

    const float available = budget + elapsed * ((float)DC_FLUID_PHASES / (float)interval);
    const uint32_t count = (uint32_t)available;
    return (dc_fluid_schedule_t){phase, count, (phase + count) % DC_FLUID_PHASES,
        timed ? (float)interval : 1.f, available - (float)count};
}
