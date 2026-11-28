/* Proposals are dead between MPM passes. Reuse their storage for parallel
   fixed-point grain corrections without allocating a second particle pool. */
#define DC_GRAIN_ACCESS
#include "rigid_grain_storage.glsl"
layout(set=0,binding=21,std430) buffer GrainScratch { uint grain_scratch[]; };
layout(set=0,binding=1,std430) readonly buffer GrainTerrain { uvec2 grain_cells[]; };
const uint GRAIN_PREPARE=15u,GRAIN_POSITION=16u,GRAIN_VELOCITY=17u;
uint grain_reference(uint c) { return DC_DATA(c+2u)==3u ? DC_DATA(c+7u) : 0u; }
uint grain_scratch_read(uint index,uint word) {
    uint at=(index & ~HYDRO_PEER)*4u+word;
    if ((index & HYDRO_PEER)!=0u) {
        HydroWords scratch=HydroWords(hydro_address(DC_HYDRO_GRAIN_SCRATCH));
        return scratch.words[at];
    }
    return grain_scratch[at];
}
uint grain_scratch_exchange(uint index,uint word,uint value) {
    uint at=(index & ~HYDRO_PEER)*4u+word;
    if ((index & HYDRO_PEER)!=0u) {
        HydroWords scratch=HydroWords(hydro_address(DC_HYDRO_GRAIN_SCRATCH));
        return atomicExchange(scratch.words[at],value);
    }
    return atomicExchange(grain_scratch[at],value);
}
void grain_scratch_add(uint index,uint word,uint value) {
    uint at=(index & ~HYDRO_PEER)*4u+word;
    if ((index & HYDRO_PEER)!=0u) {
        HydroWords scratch=HydroWords(hydro_address(DC_HYDRO_GRAIN_SCRATCH));
        atomicAdd(scratch.words[at],value);
        return;
    }
    atomicAdd(grain_scratch[at],value);
}
vec2 grain_position(uint c,uint index,Body b) {
    Particle grain=read_grain(index);
    return anchor(c+22u,b)-vec2(int(DC_DATA(c+26u)),int(DC_DATA(c+27u)))/65536.0+
        vec2(grain.x_fp,grain.y_fp)/65536.0;
}
void grain_correction(uint index,vec2 change) {
    ivec2 fp=ivec2(round(clamp(change,vec2(-2),vec2(2))*SUM_SCALE));
    if(!SKIP_ZERO_REDUCTIONS||fp.x!=0) grain_scratch_add(index,0u,uint(fp.x));
    if(!SKIP_ZERO_REDUCTIONS||fp.y!=0) grain_scratch_add(index,1u,uint(fp.y));
}
bool grain_static_at(uint c,vec2 local) {
    ivec2 cell=ivec2(floor(local)),delta=cell>>6;
    uvec2 x,y;
    if(!shifted_chunk(uvec2(DC_DATA(c+28u),DC_DATA(c+29u)),delta.x,x) ||
       !shifted_chunk(uvec2(DC_DATA(c+30u),DC_DATA(c+31u)),delta.y,y)) return true;
    ivec2 tile;
    uint page=0u;
    if(relative_chunk(x,params.origin_x,tile.x) && relative_chunk(y,params.origin_y,tile.y) &&
       all(greaterThanEqual(tile,ivec2(-33554432))) && all(lessThanEqual(tile,ivec2(33554431))))
        page=hydro_page(tile*64+(cell&63));
#ifdef DC_RUNTIME_BODY_STORAGE
    if(page==0u) {
        uint base=structural_base();
        page=cached_chunk_page(base,DC_DATA(base+1u),x,y);
    }
#endif
    if(page==0u) return true;
    uint material=hydro_cell(hydro_atlas(cell,page)).x & 65535u;
    return material==1u||material==7u||material==8u||material==9u;
}
void grain_phase(uint i,uint phase) {
    if(DC_DATA(sb()+3u)!=0u||i>=DC_DATA(cb())) return;
    uint c=cb()+12u+i*32u,ref=grain_reference(c);
    if(ref==0u) return;
    uint index=ref-1u;
    if(phase==GRAIN_PREPARE) {
        for(uint k=0u;k<4u;k++) grain_scratch_exchange(index,k,0u);
        return;
    }
    uint epoch=params.mode>>8u;
    if(grain_scratch_exchange(index,2u,epoch)==epoch) return;
    vec2 change=vec2(int(grain_scratch_read(index,0u)),int(grain_scratch_read(index,1u)))/SUM_SCALE;
    Particle p=read_grain(index);
    if(phase==GRAIN_POSITION) {
        /* Keep a particle inside the one-cell halo supported by MPM's rebin.
           Occupancy is intentionally ignored: it is the moving collider. */
        uint cell=index%4096u;
        vec2 low=vec2(cell%64u,cell/64u)-vec2(0.49);
        vec2 old=vec2(p.x_fp,p.y_fp)/65536.0;
        vec2 next=clamp(old+change,low,low+vec2(1.98));
        if(grain_static_at(c,vec2(next.x,old.y))) next.x=old.x;
        if(grain_static_at(c,vec2(old.x,next.y))) next.y=old.y;
        if(grain_static_at(c,next)) next=old;
        change=next-old;
        p.x_fp=int(round(next.x*65536.0));p.y_fp=int(round(next.y*65536.0));
        change/=DT;
    }
    vec2 v=clamp(vec2(p.vx_fp,p.vy_fp)/65536.0+change,vec2(-2),vec2(2));
    p.vx_fp=int(round(v.x*65536.0));p.vy_fp=int(round(v.y*65536.0));
    if ((index & HYDRO_PEER)!=0u)
        GrainParticles(hydro_address(DC_HYDRO_PARTICLES)).records[index & ~HYDRO_PEER]=p;
    else particles[index]=p;
    grain_scratch_exchange(index,0u,0u);grain_scratch_exchange(index,1u,0u);
}
