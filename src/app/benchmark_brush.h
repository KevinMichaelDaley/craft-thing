#ifndef DC_APP_BENCHMARK_BRUSH_H
#define DC_APP_BENCHMARK_BRUSH_H
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#define DC_BENCHMARK_WARMUP_FRAMES 8u
typedef struct {uint32_t x,y,radius;} dc_benchmark_brush_t;
/** Parse a diagnostic visible-coordinate brush, preserving output on failure. */
static inline bool dc_benchmark_brush_parse(const char *text,uint32_t width,uint32_t height,dc_benchmark_brush_t *out) {
    if(!text||!out||!width||!height) return false;
    uint32_t values[3];const char *at=text;
    for(uint32_t i=0;i<3;i++) {
        if(*at<'0'||*at>'9') return false;
        errno=0;char *end=NULL;unsigned long value=strtoul(at,&end,10);
        if(errno==ERANGE||value>UINT32_MAX||(i<2?*end!=',':*end!='\0')) return false;
        values[i]=(uint32_t)value;at=i<2?end+1:end;
    }
    if(values[0]>=width||values[1]>=height||values[2]>32u) return false;
    *out=(dc_benchmark_brush_t){values[0],values[1],values[2]};return true;
}
/** A finite pour occurs once; the existing erosion workload repeats. */
static inline bool dc_benchmark_brush_due(bool repeating,uint32_t frame) {
    return repeating?frame>=DC_BENCHMARK_WARMUP_FRAMES:frame==DC_BENCHMARK_WARMUP_FRAMES;
}
#endif
