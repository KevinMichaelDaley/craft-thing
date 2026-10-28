#ifndef DC_BONFIRE_LAYOUT_H
#define DC_BONFIRE_LAYOUT_H
/* Immutable authored test-scene layout, never a runtime body capacity. */
#define DC_BONFIRE_KINDLING_COUNT 12u
#define DC_BONFIRE_LOG_COUNT 8u
#define DC_BONFIRE_PIECE_COUNT 21u
#define DC_BONFIRE_LOG_FIRST 13u
#ifdef __STDC_VERSION__
#include <stdint.h>
#define DC_BONFIRE_RECT(x,y,w,h) {x,y,w,h}
static const uint32_t dc_bonfire_rects[DC_BONFIRE_PIECE_COUNT][4]={
#else
#define DC_BONFIRE_RECT(x,y,w,h) uvec4(x,y,w,h)
const uvec4 dc_bonfire_rects[DC_BONFIRE_PIECE_COUNT]=uvec4[DC_BONFIRE_PIECE_COUNT](
#endif
    DC_BONFIRE_RECT(144u,90u,2u,2u),
    DC_BONFIRE_RECT(148u,90u,2u,2u),
    DC_BONFIRE_RECT(152u,90u,2u,2u),
    DC_BONFIRE_RECT(166u,90u,2u,2u),
    DC_BONFIRE_RECT(170u,90u,2u,2u),
    DC_BONFIRE_RECT(174u,90u,2u,2u),
    DC_BONFIRE_RECT(146u,87u,2u,2u),
    DC_BONFIRE_RECT(151u,87u,2u,2u),
    DC_BONFIRE_RECT(156u,87u,2u,2u),
    DC_BONFIRE_RECT(162u,87u,2u,2u),
    DC_BONFIRE_RECT(167u,87u,2u,2u),
    DC_BONFIRE_RECT(172u,87u,2u,2u),
    DC_BONFIRE_RECT(156u,91u,8u,1u),
    DC_BONFIRE_RECT(146u,83u,8u,3u),
    DC_BONFIRE_RECT(158u,83u,8u,3u),
    DC_BONFIRE_RECT(170u,83u,8u,3u),
    DC_BONFIRE_RECT(150u,78u,10u,3u),
    DC_BONFIRE_RECT(164u,78u,10u,3u),
    DC_BONFIRE_RECT(146u,73u,12u,3u),
    DC_BONFIRE_RECT(166u,73u,12u,3u),
    DC_BONFIRE_RECT(154u,68u,16u,3u)
#ifdef __STDC_VERSION__
};
static inline int dc_bonfire_fixture_piece(uint32_t x,uint32_t y) {
#else
);
int dc_bonfire_fixture_piece(uint x,uint y) {
#endif
    for(int i=0;i<21;i++) {
        if(x>=dc_bonfire_rects[i][0]&&x<dc_bonfire_rects[i][0]+dc_bonfire_rects[i][2]&&
           y>=dc_bonfire_rects[i][1]&&y<dc_bonfire_rects[i][1]+dc_bonfire_rects[i][3]) return i;
    }
    return -1;
}
#undef DC_BONFIRE_RECT
#endif
