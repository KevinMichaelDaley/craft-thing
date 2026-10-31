#ifndef DC_APP_BENCHMARK_CLOCK_H
#define DC_APP_BENCHMARK_CLOCK_H

static inline float dc_benchmark_step_seconds(float elapsed_seconds,float tick_seconds) {
    /* Measure one 60 Hz world step per displayed frame. Wall time is still
     * included in the frame measurement, but cannot enqueue catch-up work. */
    (void)elapsed_seconds;
    return tick_seconds;
}

#endif
