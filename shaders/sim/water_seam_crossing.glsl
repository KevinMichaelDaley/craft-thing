#ifdef DC_WATER_SEAM_PLAN
#include "../../include/dungeoncraft/liquid_seam_layout.h"
layout(set=0,binding=DC_WATER_SEAM_CROSSING_BINDING,std430) buffer Crossings { uvec2 crossings[]; };
#endif

void record_water_crossing(ivec2 source, ivec2 target, uint amount) {
#ifdef DC_WATER_SEAM_PLAN
    bool vertical = source.y != target.y;
    uint axis = vertical ? DC_WATER_SEAM_VERTICAL : DC_WATER_SEAM_HORIZONTAL;
    if (amount == 0u || params.seam_axis != axis) return;
    int from = vertical ? source.y : source.x, to = vertical ? target.y : target.x;
    if ((from < int(DC_WATER_SEAM_SIDE)) == (to < int(DC_WATER_SEAM_SIDE))) return;
    atomicAdd(crossings[atlas_index(source, page_at(source))].x, amount);
    atomicAdd(crossings[atlas_index(target, page_at(target))].y, amount);
#endif
}
