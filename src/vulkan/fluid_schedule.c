#include "fluid_schedule.h"

dc_fluid_schedule_t dc_gpu_plan_fluid_tick(uint32_t interval, uint32_t phase, float budget,
    float tick_scale, bool timed) {
    (void)interval;
    (void)tick_scale;
    (void)timed;
    return (dc_fluid_schedule_t){.first_phase = phase, .next_phase = phase, .remaining_budget = budget};
}
