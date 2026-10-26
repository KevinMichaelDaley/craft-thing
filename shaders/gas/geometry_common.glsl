#include "storage.glsl"
#include "../../include/dungeoncraft/terrain_flags.h"
float geometry_void(uint index,uint atlas) {
    uint packed=cells[atlas].x,material=packed&65535u;
    bool solid=material!=0u&&material!=3u&&((packed&(DC_CELL_LOADED_TERRAIN<<DC_CELL_FLAGS_SHIFT))!=0u||
        material==1u||material==6u||material==7u||material==8u||material==9u);
    if(solid) return 0.;
    return max(0.,1.-float(min(65536u,occupancy[2u*params.width*params.height+index]))/65536.-
        clamp(mpm[index].w,0.,1.)-float(cells[atlas].y)/65536.);
}
vec4 geometry_metadata(uint index) {return (params.bank&1u)==0u?cache1[index].hydro:cache0[index].hydro;}
float geometry_available(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))||gas_atlas(p)==0xffffffffu) return 0.;
    return geometry_metadata(uint(p.y)*params.width+uint(p.x)).x;
}
ivec2 geometry_direction(uint d) {return d==0u?ivec2(1,0):(d==1u?ivec2(-1,0):(d==2u?ivec2(0,1):ivec2(0,-1)));}
bool geometry_open(ivec2 p,uint d) {return gas_gate_open((d&1u)==0u?p:p+geometry_direction(d),d/2u);}
float geometry_route(ivec2 p) {
    float total=0.;for(uint d=0u;d<4u;d++) if(geometry_open(p,d)) total+=geometry_available(p+geometry_direction(d));return total;
}
void geometry_mark(uint index,bool changed) {
    if((params.bank&1u)==0u) cache1[index].metric.w=changed?1.:0.;else cache0[index].metric.w=changed?1.:0.;
}
