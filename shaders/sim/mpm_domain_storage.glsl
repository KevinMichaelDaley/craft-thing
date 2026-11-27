#include "../../include/dungeoncraft/terrain_flags.h"
layout(set=0,binding=4,std430) buffer DomainSupport { uint domain_dirty[]; };
layout(set=0,binding=5,std430) buffer GrainSources { uint grain_sources[]; };
layout(set=0,binding=6,std430) buffer NextGrainSources { uint next_grain_sources[]; };

/* Keep the original particle slot through both gathers. Paired owners use
   this correspondence when moving a reactive parcel between their arenas. */
void reactive_particle_gather(uint pool,uint source,uint target,bool occupied) {
    next_grain_sources[target]=occupied?grain_sources[source]:0u;
}
void reactive_particle_commit(uint pool,uint target) {
    grain_sources[target]=next_grain_sources[target];
}
void mpm_support_changed(ivec2 cell,uint before,uint after) {
    if(DC_CELL_COMPONENT_MEMBER(before)==DC_CELL_COMPONENT_MEMBER(after)) return;
    for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) {
        if(dx==0&&dy==0) continue;
        ivec2 adjacent=cell+ivec2(dx,dy);
        uint page=page_at(adjacent);
        if(page==0u) continue;
        uint packed=chunk_cells[(page-1u)*4096u+local_cell(adjacent)].x;
        if((packed & (DC_CELL_LOADED_TERRAIN << DC_CELL_FLAGS_SHIFT))!=0u &&
           (packed & 65535u)!=0u && (packed & 65535u)!=3u)
            atomicOr(domain_dirty[page-1u],1u);
    }
}
