#ifndef DC_PRESENTATION_H
#define DC_PRESENTATION_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t x,y,width,height;} dc_presentation_focus_t;
typedef struct {uint32_t x,y,width,height,left,top,scale;} dc_presentation_layout_t;
/** Select a display crop without changing the simulated or rendered grid. */
static inline bool dc_presentation_layout(uint32_t width,uint32_t height,uint32_t window_width,
    uint32_t window_height,uint32_t zoom,dc_presentation_focus_t focus,dc_presentation_layout_t *out) {
    if(!out||!width||!height||!zoom) return false;
    dc_presentation_layout_t r={.width=width,.height=height,.scale=zoom};
    if(focus.width) {
        if(!focus.height||focus.x>=width||focus.y>=height||focus.width>width-focus.x||focus.height>height-focus.y) return false;
        r.x=focus.x;r.y=focus.y;r.width=focus.width;r.height=focus.height;
        r.scale=window_width/r.width;if(window_height/r.height<r.scale) r.scale=window_height/r.height;
    }
    if(!r.scale||(uint64_t)r.width*r.scale>window_width||(uint64_t)r.height*r.scale>window_height) return false;
    r.left=(window_width-r.width*r.scale)/2;r.top=(window_height-r.height*r.scale)/2;*out=r;return true;
}
/** Map a presented point back into the original full-resolution visible grid. */
static inline bool dc_presentation_pick(dc_presentation_layout_t layout,uint32_t sx,uint32_t sy,uint32_t *x,uint32_t *y) {
    if(!x||!y||!layout.scale||sx<layout.left||sy<layout.top||
       (sx-layout.left)/layout.scale>=layout.width||(sy-layout.top)/layout.scale>=layout.height) return false;
    *x=layout.x+(sx-layout.left)/layout.scale;*y=layout.y+(sy-layout.top)/layout.scale;return true;
}
/** Horizontal split presentation: band i owns screen columns
 * [i*w/count,(i+1)*w/count). Every band letterboxes its own crop inside
 * its band with integer scale, exactly like a focused presentation
 * clipped to that band. A zero-width crop shows the complete grid. */
static inline bool dc_presentation_split_layout(uint32_t width,uint32_t height,
    uint32_t window_width,uint32_t window_height,uint32_t zoom,
    const dc_presentation_focus_t *crops,uint32_t count,
    dc_presentation_layout_t *out) {
    if(!out||!width||!height||!zoom||!window_width||!window_height) return false;
    if(count==0u||(count>0u&&!crops)) return false;
    for(uint32_t i=0u;i<count;i++) {
        uint32_t band_left=window_width/count*i;
        uint32_t band_right=i+1u==count?window_width:window_width/count*(i+1u);
        uint32_t band_w=band_right-band_left,band_h=window_height;
        dc_presentation_focus_t focus=crops[i];
        dc_presentation_layout_t r={.width=width,.height=height,.scale=zoom,
            .x=0u,.y=0u};
        if(focus.width) {
            if(!focus.height||focus.x>=width||focus.y>=height||
               focus.width>width-focus.x||focus.height>height-focus.y) return false;
            r.x=focus.x;r.y=focus.y;r.width=focus.width;r.height=focus.height;
            r.scale=band_w/r.width;
            if(band_h/r.height<r.scale) r.scale=band_h/r.height;
        }
        if(!r.scale||(uint64_t)r.width*r.scale>band_w||
           (uint64_t)r.height*r.scale>band_h) return false;
        r.left=band_left+(band_w-r.width*r.scale)/2;
        r.top=(band_h-r.height*r.scale)/2;
        out[i]=r;
    }
    return true;
}
/** Map a presented point back into the grid through its owning band. */
static inline bool dc_presentation_split_pick(const dc_presentation_layout_t *layouts,
    uint32_t count,uint32_t sx,uint32_t sy,uint32_t *x,uint32_t *y) {
    if(!layouts||!count||!x||!y) return false;
    for(uint32_t i=0u;i<count;i++)
        if(dc_presentation_pick(layouts[i],sx,sy,x,y)) return true;
    return false;
}
#endif
