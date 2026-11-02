#ifndef DC_APP_PLAYER_CLOCK_H
#define DC_APP_PLAYER_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {uint32_t ticks;double remainder;} dc_player_clock_plan_t;
/** Preserve fixed 60Hz numerical steps and sub-tick remainder. Under overload,
 * admit at most one step per display and discard excess wall-clock debt, so
 * slow rendering cannot recursively multiply physics work and input latency.
 * Simulation explicitly runs slower than wall time when hardware is overloaded. */
bool dc_player_clock_plan(double accumulator,double tick,bool forced,dc_player_clock_plan_t *out);

#endif
