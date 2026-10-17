/* Leaf emission cooperates across the workgroup. Particle slots are reserved
 * before remnant publication and released if that transaction cannot commit. */
const uint LEAF_RESERVATION=0x80000000u;
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
    for(uint pixel=lane;pixel<body_pieces(source);pixel+=32u) {
        if(piece_material(source,pixel)!=5u||fracture_labels[pixel]!=0u) continue;
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
        fracture_labels[pixel]=LEAF_RESERVATION|(p+1u);
        atomicOr(leaf_changed,1u);
    }
    memoryBarrierBuffer();barrier();
}
shared Body leaf_impact_source;
void leaf_finish(Body source,bool committed);
shared uint leaf_impact_mask[2],leaf_impact_committed;
void leaf_impact_breaks() {
    uint lane=gl_LocalInvocationIndex;
    if(lane<2u) leaf_impact_mask[lane]=0u;
    barrier();
    for(uint slot=lane;slot<BODY_CAPACITY;slot+=32u) {
        uint p=graph()+38000u+slot*4u;Body b=bodies[slot];
        if(b.enabled!=0u&&pixel_body(b)&&data[p+3u]==b.id&&(data[p+2u]&256u)!=0u)
            atomicOr(leaf_impact_mask[slot/32u],1u<<(slot%32u));
    }
    barrier();
    for(uint word=0u;word<2u;word++) for(uint mask=leaf_impact_mask[word];mask!=0u;mask&=mask-1u) {
        uint slot=word*32u+uint(findLSB(mask));
        if(lane==0u) {
            leaf_impact_source=bodies[slot];
            leaf_impact_committed=0u;
        }
        barrier();
        {
            for(uint pixel=lane;pixel<body_pieces(leaf_impact_source);pixel+=32u) fracture_labels[pixel]=0u;
            memoryBarrierBuffer();barrier();
            leaf_reserve(leaf_impact_source);
            if(lane==0u&&leaf_changed!=0u)
                leaf_impact_committed=uint(pixel_fragment_source(slot,leaf_impact_source,2u,0.,max(1u,leaf_impact_source.reserved1)));
            memoryBarrierBuffer();barrier();
            leaf_finish(leaf_impact_source,leaf_impact_committed!=0u);
            if(lane==0u&&leaf_impact_committed!=0u) data[graph()+38000u+slot*4u+2u]&=255u;
            memoryBarrierBuffer();barrier();
            if(leaf_impact_committed!=0u) {
                uint pool=graph()+40000u;
                uint first=BODY_STORAGE_WORDS+pool+16u+data[pool]*color_capacity();
                for(uint word=lane;word<data[pool+1u];word+=32u) shape_color_resolve(first+word);
            }
            memoryBarrierBuffer();barrier();
        }
    }
}
void leaf_finish(Body source,bool committed) {
    uint lane=gl_LocalInvocationIndex;
    for(uint pixel=lane;pixel<body_pieces(source);pixel+=32u) {
        uint label=fracture_labels[pixel];
        if(label==0xffffffffu||(label&LEAF_RESERVATION)==0u) continue;
        uint p=(label&~LEAF_RESERVATION)-1u;
        if(!committed) {debris_particles[p].mass_fp=0u;continue;}
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
        particle.grain_fp=16384u;particle.material=10u;particle.flags=0u;
        debris_particles[p]=particle;atomicAdd(debris_counts[slot],1u);
        atomicExchange(cells[c].x,10u);
        if((p%8192u)<4096u) {
            uint word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
            terrain_colors[c]=shape_color_get(word,pixel%8u);
        }
        uint view=uint(cell.y)*params.width+uint(cell.x);
        atomicCompSwap(debris_occupancy[view],source.id,0u);
        atomicCompSwap(debris_occupancy[params.width*params.height+view],source.id,0u);
    }
}
