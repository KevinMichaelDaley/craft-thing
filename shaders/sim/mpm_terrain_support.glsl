#include "rigid_body.glsl"
#include "../../include/dungeoncraft/terrain_flags.h"
layout(set=0,binding=4,std430) buffer SupportStorage {Body support_bodies[BODY_CAPACITY];uint support_data[];};
uint pixel_word_at(uint index) {return support_data[index-BODY_STORAGE_WORDS];}

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
           (material!=1u&&material!=7u&&material!=8u&&material!=9u)) continue;
        uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
        uint base=8u+2u*w*h+4096u*8u+12u+8192u*32u+16u+64u*32u+8192u*16u;
        atomicOr(support_data[base+STRUCTURAL_COMPONENT_DIRTY_WORD],1u);
        return;
    }
}
