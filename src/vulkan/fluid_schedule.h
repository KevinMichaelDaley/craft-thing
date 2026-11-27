#ifndef DUNGEONCRAFT_FLUID_SCHEDULE_H
#define DUNGEONCRAFT_FLUID_SCHEDULE_H

#include <stdbool.h>
#include <stdint.h>

enum { DC_FLUID_PHASES = 6u };

typedef struct {
    uint32_t first_phase, phase_count, next_phase;
    float step_scale, remaining_budget;
} dc_fluid_schedule_t;

/** Preview one native tick's water phases without consuming the owner's phase or budget. */
dc_fluid_schedule_t dc_gpu_plan_fluid_tick(uint32_t interval, uint32_t phase, float budget,
    float tick_scale, bool timed);

#endif
