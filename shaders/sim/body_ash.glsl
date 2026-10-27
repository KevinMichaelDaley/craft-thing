#include "../../include/dungeoncraft/body_ash_layout.h"
bool body_ash_mode() {return params.origin_y.y==DC_BODY_ASH_TRANSACTION_TAG;}
#ifdef DC_RUNTIME_BODY_STORAGE
uvec2 body_ash_handle(Body source,uint pixel) {
    uint word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
    return reactive_shape_read(word-color_pool_word(),pixel%8u,color_capacity());
}
uint body_ash_mass(Body source,uint pixel) {
    if(!reactive_particles_enabled()||!pixel_body(source)) return 0u;
    uvec2 handle=body_ash_handle(source,pixel);if(!reactive_handle_valid(handle)) return 0u;
    ReactiveWords entry=reactive_entry_buffer(handle.x);uint s=FR_ENTRY_STATE,m=entry.words[s];
    if(entry.words[FR_ENTRY_OWNER]!=2u||entry.words[FR_ENTRY_OWNER+1u]!=source.id||
       (m!=7u&&m!=8u&&m!=9u&&m!=12u)) return 0u;
    if((entry.words[s+9u]|entry.words[s+10u]|entry.words[s+13u]|entry.words[s+14u]|entry.words[s+15u]|entry.words[s+16u])!=0u) return 0u;
    if(entry.words[s+11u]>0xffffffffu-entry.words[s+12u]) return 0u;
    return entry.words[s+11u]+entry.words[s+12u];
}
void body_ash_reserve(Body source) {
    for(uint pixel=gl_LocalInvocationIndex;pixel<body_pieces(source);pixel+=gl_WorkGroupSize.x) {
        if(piece_material(source,pixel)==0u||body_ash_mass(source,pixel)==0u) continue;
        ivec2 cell=ivec2(floor(leaf_position(source,pixel)));
        if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) continue;
        uint page=pages[uint(cell.y/64)*((params.width+63u)/64u)+uint(cell.x/64)];if(page==0u) continue;
        uint c=(page-1u)*4096u+uint(cell.y%64)*64u+uint(cell.x%64),material=cells[c].x&65535u;
        if(material!=0u&&material!=13u) continue;
        uint p=(page-1u)*8192u+c%4096u;
        if(atomicCompSwap(debris_particles[p].mass_fp,0u,0xffffffffu)!=0u) {
            p+=4096u;if(atomicCompSwap(debris_particles[p].mass_fp,0u,0xffffffffu)!=0u) continue;
        }
        if(reactive_handle_valid(reactive_particle_read(0u,p))) {debris_particles[p].mass_fp=0u;continue;}
        DC_FRAGMENT_LABEL(pixel)=LEAF_RESERVATION|(p+1u);atomicOr(carve_hits,1u);
    }
    memoryBarrierBuffer();barrier();
}
void body_ash_finish(Body source,bool committed) {
    for(uint pixel=gl_LocalInvocationIndex;pixel<body_pieces(source);pixel+=gl_WorkGroupSize.x) {
        uint label=DC_FRAGMENT_LABEL(pixel);if(label==0xffffffffu||(label&LEAF_RESERVATION)==0u) continue;
        uint p=(label&~LEAF_RESERVATION)-1u;
        if(!committed) {debris_particles[p].mass_fp=0u;continue;}
        uvec2 handle=body_ash_handle(source,pixel);ReactiveWords entry=reactive_entry_buffer(handle.x);uint s=FR_ENTRY_STATE;
        vec2 position=leaf_position(source,pixel);ivec2 cell=ivec2(floor(position));
        vec2 offset=position-vec2(source.x_fp,source.y_fp)/65536.-source.center;
        vec2 velocity=vec2(source.vx_fp,source.vy_fp)/65536.+source.angular_velocity*vec2(-offset.y,offset.x);
        uvec2 id=uvec2(entry.words[s+2u],entry.words[s+3u]);uint mass=body_ash_mass(source,pixel);
        uint radius=max(1u,uint(sqrt(float(mass)/65536./.25/3.14159265)*65536.));
        debris_particles[p]=StructuralParticle(int(round((position.x-float((cell.x/64)*64))*65536.)),
            int(round((position.y-float((cell.y/64)*64))*65536.)),int(round(velocity.x*65536.)),int(round(velocity.y*65536.)),
            1.,0.,0.,1.,id.x,id.y,mass,radius,13u,0u);
        entry.words[s]=13u;entry.words[s+1u]=16u;entry.words[s+8u]=entry.words[s+11u];
        reactive_owner_write(handle.x,3u,uvec4(id,id),0u);
        reactive_particle_write(0u,p,handle);reactive_particle_write(1u,p,uvec2(0));
        uint word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
        reactive_shape_write(word-color_pool_word(),pixel%8u,color_capacity(),uvec2(0));
        uint c=(p/8192u)*4096u+p%4096u;atomicExchange(cells[c].x,13u);atomicAdd(debris_counts[p/8192u],1u);
        uint view=uint(cell.y)*params.width+uint(cell.x);
        atomicCompSwap(debris_occupancy[view],source.id,0u);atomicCompSwap(debris_occupancy[params.width*params.height+view],source.id,0u);
    }
    memoryBarrierBuffer();barrier();
}
#else
void body_ash_reserve(Body source) {}
void body_ash_finish(Body source,bool committed) {}
#endif
