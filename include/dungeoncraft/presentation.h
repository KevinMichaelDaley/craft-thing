#ifndef DC_PRESENTATION_H
#define DC_PRESENTATION_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t x,y,width,height;} dc_presentation_focus_t;
typedef struct {uint32_t x,y,width,height,left,top,scale;} dc_presentation_layout_t;
/** Select a display crop without changing the simulated or rendered grid. */
static inline bool dc_presentation_layout(uint32_t width,uint32_t height,uint32_t window_width,
    uint32_t window_height,uint32_t zoom,dc_presentation_focus_t focus,dc_presentation_layout_t *out) {
    (void)width;(void)height;(void)window_width;(void)window_height;(void)zoom;(void)focus;(void)out;return false;
}
/** Map a presented point back into the original full-resolution visible grid. */
static inline bool dc_presentation_pick(dc_presentation_layout_t layout,uint32_t sx,uint32_t sy,uint32_t *x,uint32_t *y) {
    (void)layout;(void)sx;(void)sy;(void)x;(void)y;return false;
}
#endif
