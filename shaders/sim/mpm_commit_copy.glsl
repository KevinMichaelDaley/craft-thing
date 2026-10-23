/* Metadata observes the old particle pool before lanes cooperate on its copy. */
layout(set=0,binding=19,std430) buffer PackedParticles {uvec4 packed_particles[];};
layout(set=0,binding=22,std430) readonly buffer PackedOutput {uvec4 packed_output[];};
void coalesced_commit(uint tile,uint tiles_x) {
    uvec2 first=uvec2(tile%tiles_x,tile/tiles_x)*16u;
    uint page=pages[(first.y/64u)*((params.width+63u)/64u)+first.x/64u];
    if(page==0u) return;
    ivec2 cell=ivec2(first+gl_LocalInvocationID.xy);
    if(in_grid(cell)) commit_cell(cell);
    memoryBarrierBuffer();barrier();
    uint columns=min(16u,params.width-first.x),rows=min(16u,params.height-first.y);
    for(uint item=gl_LocalInvocationIndex;item<2u*16u*56u;item+=256u) {
        uint layer=item/(16u*56u),row=(item/56u)%16u,vector=item%56u;
        if(row>=rows||vector*4u>=columns*14u) continue;
        uint particle=(page-1u)*8192u+layer*4096u+(first.y%64u+row)*64u+first.x%64u;
        uint index=particle*14u/4u+vector;
        uvec4 value=packed_output[index];
        if(vector*4u+4u>columns*14u) {
            uvec4 old=packed_particles[index];
            for(uint lane=0u;lane<4u;lane++) if(vector*4u+lane>=columns*14u) value[lane]=old[lane];
        }
        packed_particles[index]=value;
    }
}
