#include "rigid_body.glsl"
#include "../../include/dungeoncraft/terrain_flags.h"
#ifndef DC_RUNTIME_BODY_STORAGE
layout(set=0,binding=4,std430) buffer SupportStorage {Body support_bodies[BODY_CAPACITY];uint support_data[];};
#endif
#ifndef DC_RUNTIME_BODY_STORAGE
#undef DC_PIXEL_WORD
#define DC_PIXEL_WORD(index) DC_SUPPORT_DATA((index)-BODY_STORAGE_WORDS)
#endif
uint pixel_word_at(uint index) {return DC_PIXEL_WORD(index);}

/* Only changing support next to held terrain invalidates the rigid graph. */
void mpm_support_changed(ivec2 cell,uint before,uint after) {
    bool was_solid=before!=0u&&before!=3u,is_solid=after!=0u&&after!=3u;
    if(was_solid==is_solid) return;
    for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) {
        if(dx==0&&dy==0) continue;
        ivec2 neighbor=cell+ivec2(dx,dy);uint page=page_at(neighbor);
        if(page==0u) continue;
        uint packed=chunk_cells[(page-1u)*4096u+local_cell(neighbor)].x,material=packed&65535u;
        if((packed&(DC_CELL_LOADED_TERRAIN<<DC_CELL_FLAGS_SHIFT))==0u||
           material==0u||material==3u) continue;
        uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
        uint base=8u+2u*w*h+DC_LAYOUT_PAIR_CAPACITY*8u+12u+DC_LAYOUT_CONTACT_CAPACITY*32u+16u+64u*32u+DC_LAYOUT_CONTACT_CAPACITY*16u;
        atomicOr(DC_SUPPORT_DATA(base+STRUCTURAL_COMPONENT_DIRTY_WORD),1u);
        return;
    }
}
