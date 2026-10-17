/* Proposals are dead between MPM passes. Reuse their storage for parallel
   fixed-point grain corrections without allocating a second particle pool. */
struct Particle {
    int x_fp, y_fp, vx_fp, vy_fp;
    float f00, f01, f10, f11;
    uint id_lo, id_hi, mass_fp, grain_fp, material, flags;
};
layout(set=0,binding=19,std430) buffer Particles { Particle particles[]; };
layout(set=0,binding=21,std430) buffer GrainScratch { uint grain_scratch[]; };
layout(set=0,binding=1,std430) readonly buffer GrainTerrain { uvec2 grain_cells[]; };
const uint GRAIN_PREPARE=15u,GRAIN_POSITION=16u,GRAIN_VELOCITY=17u;
uint grain_reference(uint c) { return data[c+2u]==3u ? data[c+7u] : 0u; }
vec2 grain_position(uint c,uint index) {
    return anchor(c+22u)-vec2(int(data[c+26u]),int(data[c+27u]))/65536.0+
        vec2(particles[index].x_fp,particles[index].y_fp)/65536.0;
}
void grain_correction(uint index,vec2 change) {
    ivec2 fp=ivec2(round(clamp(change,vec2(-2),vec2(2))*SUM_SCALE));
    atomicAdd(grain_scratch[index*4u],uint(fp.x));
    atomicAdd(grain_scratch[index*4u+1u],uint(fp.y));
}
bool grain_static_at(vec2 point) {
    ivec2 cell=ivec2(floor(point));
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) return true;
    uint page=pages[uint(cell.y/64)*((params.width+63u)/64u)+uint(cell.x/64)];
    if(page==0u) return true;
    uint material=grain_cells[(page-1u)*4096u+uint(cell.y%64)*64u+uint(cell.x%64)].x&65535u;
    return material==1u||material==7u||material==8u||material==9u;
}
void grain_phase(uint i) {
    if(data[sb()+3u]!=0u||i>=data[cb()]) return;
    uint c=cb()+12u+i*32u,ref=grain_reference(c);
    if(ref==0u) return;
    uint index=ref-1u,s=index*4u;
    if(PHASE==GRAIN_PREPARE) {
        for(uint k=0u;k<4u;k++) atomicExchange(grain_scratch[s+k],0u);
        return;
    }
    uint epoch=params.mode>>8u;
    if(atomicExchange(grain_scratch[s+2u],epoch)==epoch) return;
    vec2 change=vec2(int(grain_scratch[s]),int(grain_scratch[s+1u]))/SUM_SCALE;
    Particle p=particles[index];
    if(PHASE==GRAIN_POSITION) {
        /* Keep a particle inside the one-cell halo supported by MPM's rebin.
           Occupancy is intentionally ignored: it is the moving collider. */
        uint cell=index%4096u;
        vec2 low=vec2(cell%64u,cell/64u)-vec2(0.49);
        vec2 old=vec2(p.x_fp,p.y_fp)/65536.0;
        vec2 next=clamp(old+change,low,low+vec2(1.98));
        vec2 origin=grain_position(c,index)-old;
        if(grain_static_at(origin+vec2(next.x,old.y))) next.x=old.x;
        if(grain_static_at(origin+vec2(old.x,next.y))) next.y=old.y;
        if(grain_static_at(origin+next)) next=old;
        change=next-old;
        p.x_fp=int(round(next.x*65536.0));p.y_fp=int(round(next.y*65536.0));
        change/=DT;
    }
    vec2 v=clamp(vec2(p.vx_fp,p.vy_fp)/65536.0+change,vec2(-2),vec2(2));
    p.vx_fp=int(round(v.x*65536.0));p.vy_fp=int(round(v.y*65536.0));
    particles[index]=p;
    grain_scratch[s]=0u;grain_scratch[s+1u]=0u;
}
