#include "acoustic.glsl"
void main() {
    uint group=gas_index(),width=(params.width+FR_GAS_GROUP_TILE_SIZE-1u)/FR_GAS_GROUP_TILE_SIZE,height=(params.height+FR_GAS_GROUP_TILE_SIZE-1u)/FR_GAS_GROUP_TILE_SIZE;
    if(group>=width*height) return;
    if(group==0u) atomicOr(stats[15],128u|(FR_GAS_GROUP_TILE_SIZE<<8u));
    uint tile_width=(params.width+7u)/8u,tile_height=(params.height+7u)/8u;
    uvec2 origin=uvec2(group%width,group/width)*(FR_GAS_GROUP_TILE_SIZE/8u);bool needed=false;
    for(uint y=0u;y<FR_GAS_GROUP_TILE_SIZE/8u;y++) for(uint x=0u;x<FR_GAS_GROUP_TILE_SIZE/8u;x++) {
        uvec2 p=origin+uvec2(x,y);
        if(p.x<tile_width&&p.y<tile_height&&(activity[gas_tile_count()+p.y*tile_width+p.x]&FR_GAS_SOLVE_TILE_BIT)!=0u) needed=true;
    }
    if(needed) {uint at=atomicAdd(indirect[12],1u);active_tiles[3u*gas_tile_count()+at]=group;}
}
