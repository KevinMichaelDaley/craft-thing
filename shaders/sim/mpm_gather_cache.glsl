/* A one-cell halo shares immutable acceptance decisions across the tile.
 * Keep page IDs separate so every decision value is preserved unchanged. */
const uint GATHER_SIDE=18u,GATHER_CELLS=GATHER_SIDE*GATHER_SIDE;
shared uint gather_pages[GATHER_CELLS];
shared uvec2 gather_flags[GATHER_CELLS];
void gather_cache_fill(uint tile,uint tiles_x) {
    ivec2 first=ivec2(tile%tiles_x,tile/tiles_x)*16-ivec2(1);
    for(uint item=gl_LocalInvocationIndex;item<GATHER_CELLS;item+=256u) {
        ivec2 cell=first+ivec2(item%GATHER_SIDE,item/GATHER_SIDE);
        uint page=page_at(cell);uvec2 flags=uvec2(0);
        if(page!=0u) {
            uint index=(page-1u)*PARTICLES+local_cell(cell);
            flags=uvec2(gather_decision(index),gather_decision(index+CELLS));
        }
        gather_pages[item]=page;gather_flags[item]=flags;
    }
    barrier();
}
