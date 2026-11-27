#ifndef DC_CATAPULT_STARTUP_TRACE_H
#define DC_CATAPULT_STARTUP_TRACE_H

#include <SDL.h>
#include <stdio.h>

typedef struct dc_catapult_startup_trace {
    Uint64 first, last;
    double milliseconds_per_tick;
} dc_catapult_startup_trace_t;

static inline void dc_catapult_startup_mark(dc_catapult_startup_trace_t *trace,
                                           const char *phase) {
    if (!trace) return;
    /* Keep the intervals contiguous so they reconcile with the first-present total. */
    Uint64 now = SDL_GetPerformanceCounter();
    fprintf(stderr, "Catapult startup %s: %.3f ms\n", phase,
            (now - trace->last) * trace->milliseconds_per_tick);
    trace->last = now;
}

#endif
