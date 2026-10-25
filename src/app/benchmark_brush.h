#ifndef DC_APP_BENCHMARK_BRUSH_H
#define DC_APP_BENCHMARK_BRUSH_H
#include <stdbool.h>
#include <stdint.h>
#define DC_BENCHMARK_WARMUP_FRAMES 8u
typedef struct {uint32_t x,y,radius;} dc_benchmark_brush_t;
/** Parse a diagnostic visible-coordinate brush, preserving output on failure. */
static inline bool dc_benchmark_brush_parse(const char *text,uint32_t width,uint32_t height,dc_benchmark_brush_t *out) {
    (void)text;(void)width;(void)height;(void)out;return false;
}
/** A finite pour occurs once; the existing erosion workload repeats. */
static inline bool dc_benchmark_brush_due(bool repeating,uint32_t frame) {
    (void)repeating;(void)frame;return false;
}
#endif
