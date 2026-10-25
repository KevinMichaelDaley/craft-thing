/* Leaf emission cooperates across the workgroup. Particle slots are reserved
 * before remnant publication and released if that transaction cannot commit. */
const uint LEAF_RESERVATION=0x80000000u;
#include "../reactive/particle_storage.glsl"
#include "mpm_particle_mirror.glsl"
#include "leaf_color.glsl"
shared uint leaf_changed;
vec2 leaf_position(Body source,uint pixel) {
    vec2 rest=vec2(pixel_piece_cell(source,pixel))+.5;
    return vec2(source.x_fp,source.y_fp)/65536.+source.center+
        body_rotate(rest-source.center,source.angle);
}
void leaf_reserve(Body source) {
    uint lane=gl_LocalInvocationIndex;
    if(lane==0u) leaf_changed=0u;
    barrier();
    for(uint pixel=lane;pixel<body_pieces(source);pixel+=gl_WorkGroupSize.x) {
        if(piece_material(source,pixel)!=5u||DC_FRAGMENT_LABEL(pixel)!=0u) continue;
        uint state_word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
        if(!reactive_leaf_reservation_ready(state_word-color_pool_word(),pixel%8u,color_capacity())) continue;
        ivec2 cell=ivec2(floor(leaf_position(source,pixel)));
        if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) continue;
        uint page=pages[uint(cell.y/64)*((params.width+63u)/64u)+uint(cell.x/64)];
        if(page==0u) continue;
        uint c=(page-1u)*4096u+uint(cell.y%64)*64u+uint(cell.x%64);
        uint material=cells[c].x&65535u;
        if(material!=0u&&material!=10u) continue;
        uint p=(page-1u)*8192u+c%4096u;
        if(atomicCompSwap(debris_particles[p].mass_fp,0u,0xffffffffu)!=0u) {
            p+=4096u;
            if(atomicCompSwap(debris_particles[p].mass_fp,0u,0xffffffffu)!=0u) continue;
        }
        DC_FRAGMENT_LABEL(pixel)=LEAF_RESERVATION|(p+1u);
        atomicOr(leaf_changed,1u);
    }
    memoryBarrierBuffer();barrier();
}
shared Body leaf_impact_source;
void leaf_finish(Body source,bool committed);
shared uint leaf_impact_committed;
#ifdef DC_RUNTIME_BODY_STORAGE
#define LEAF_MASK(i) DC_RUNTIME_WORK(DC_RUNTIME_WORK(5u)+(i))
#else
shared uint leaf_impact_mask[2];
#define LEAF_MASK(i) leaf_impact_mask[i]
#endif
void leaf_impact_breaks() {
    uint lane=gl_LocalInvocationIndex;
    uint mask_words=(BODY_CAPACITY+31u)/32u;
    for(uint word=lane;word<mask_words;word+=gl_WorkGroupSize.x) LEAF_MASK(word)=0u;
    memoryBarrierBuffer();
    barrier();
    for(uint slot=lane;slot<BODY_CAPACITY;slot+=gl_WorkGroupSize.x) {
        uint p=body_impact_pending(slot);Body b=DC_BODY(slot);
        if(b.enabled!=0u&&pixel_body(b)&&DC_SOLVER_DATA(p+3u)==b.id&&(DC_SOLVER_DATA(p+2u)&256u)!=0u) {
#ifdef DC_RUNTIME_BODY_STORAGE
            if(body_pieces(b)>DC_RUNTIME_WORK(13u)) {
                atomicMax(DC_RUNTIME_REQUEST(9u),body_pieces(b));continue;
            }
#endif
            atomicOr(LEAF_MASK(slot/32u),1u<<(slot%32u));
        }
    }
    memoryBarrierBuffer();barrier();
    for(uint word=0u;word<mask_words;word++) for(uint mask=LEAF_MASK(word);mask!=0u;mask&=mask-1u) {
        uint slot=word*32u+uint(findLSB(mask));
        if(lane==0u) {
            leaf_impact_source=DC_BODY(slot);
            leaf_impact_committed=0u;
        }
        barrier();
        {
            for(uint pixel=lane;pixel<body_pieces(leaf_impact_source);pixel+=gl_WorkGroupSize.x) DC_FRAGMENT_LABEL(pixel)=0u;
            memoryBarrierBuffer();barrier();
            leaf_reserve(leaf_impact_source);
            if(lane==0u&&leaf_changed!=0u)
                leaf_impact_committed=uint(pixel_fragment_source(slot,leaf_impact_source,2u,0.,max(1u,leaf_impact_source.reserved1)));
            memoryBarrierBuffer();barrier();
            leaf_finish(leaf_impact_source,leaf_impact_committed!=0u);
            if(lane==0u&&leaf_impact_committed!=0u) DC_SOLVER_DATA(body_impact_pending(slot)+2u)&=255u;
            memoryBarrierBuffer();barrier();
            if(leaf_impact_committed!=0u) {
                uint pool=graph()+40000u;
                uint first=BODY_STORAGE_WORDS+pool+16u+DC_SOLVER_DATA(pool)*color_capacity();
                for(uint word=lane;word<DC_SOLVER_DATA(pool+1u);word+=gl_WorkGroupSize.x) shape_color_resolve(first+word);
            }
            memoryBarrierBuffer();barrier();
        }
    }
}
void leaf_finish(Body source,bool committed) {
    uint lane=gl_LocalInvocationIndex;
    for(uint pixel=lane;pixel<body_pieces(source);pixel+=gl_WorkGroupSize.x) {
        uint label=DC_FRAGMENT_LABEL(pixel);
        if(label==0xffffffffu||(label&LEAF_RESERVATION)==0u) continue;
        uint p=(label&~LEAF_RESERVATION)-1u;
        if(!committed) {debris_particles[p].mass_fp=0u;mpm_forget_output(p);continue;}
        vec2 position=leaf_position(source,pixel);
        ivec2 cell=ivec2(floor(position));
        uint slot=p/8192u,c=slot*4096u+p%4096u;
        vec2 offset=position-vec2(source.x_fp,source.y_fp)/65536.-source.center;
        vec2 velocity=vec2(source.vx_fp,source.vy_fp)/65536.+
            source.angular_velocity*vec2(-offset.y,offset.x);
        StructuralParticle particle;
        particle.x_fp=int(round((position.x-float((cell.x/64)*64))*65536.));
        particle.y_fp=int(round((position.y-float((cell.y/64)*64))*65536.));
        particle.vx_fp=int(round(velocity.x*65536.));particle.vy_fp=int(round(velocity.y*65536.));
        particle.f00=1;particle.f01=0;particle.f10=0;particle.f11=1;
        ivec2 rest=pixel_piece_cell(source,pixel);
        particle.id_lo=0x80000000u|(uint(rest.y)*source.width+uint(rest.x)+1u);particle.id_hi=source.id;
        particle.mass_fp=uint(round(DC_DENSITY_LEAVES*65536.));
        uint color_word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
        vec4 color=shape_color_get(color_word,pixel%8u);
        particle.grain_fp=DC_LEAF_GRAIN_RADIUS_FP;particle.material=10u;
        particle.flags=leaf_color_pack(color.w>0.?color.rgb:vec3(.08,.3,.025));
        debris_particles[p]=particle;atomicAdd(debris_counts[slot],1u);
        reactive_leaf_to_particle(color_word-color_pool_word(),pixel%8u,color_capacity(),p,uvec2(particle.id_lo,particle.id_hi));
        atomicExchange(cells[c].x,10u);
        if((p%8192u)<4096u) {
            terrain_colors[c]=vec4(leaf_color_unpack(particle.flags),1.);
        }
        uint view=uint(cell.y)*params.width+uint(cell.x);
        atomicCompSwap(debris_occupancy[view],source.id,0u);
        atomicCompSwap(debris_occupancy[params.width*params.height+view],source.id,0u);
    }
}
