/* One cooperative transaction owns the reusable fracture scratch and alternate
 * geometry bank. Only reservations/publication are serial; pixel work is not. */
#define DC_PIXEL_PROPERTY_LANES DC_STRUCTURAL_LANES
#include "pixel_properties.glsl"
shared uint pc_count,pc_available,pc_seed,pc_tail,pc_end,pc_used,pc_total,pc_next,pc_failed;
shared ivec2 pc_lows[DC_STRUCTURAL_LANES],pc_highs[DC_STRUCTURAL_LANES];
#ifndef DC_CARVE_UNION_LABELS
#define DC_CARVE_UNION_LABELS 1
#endif
#if DC_CARVE_UNION_LABELS && DC_CARVE_REUSE_CLASSIFICATION
#include "carve_union.glsl"
#endif
#ifndef DC_CARVE_PARALLEL_TILES
#define DC_CARVE_PARALLEL_TILES 1
#endif
#if DC_CARVE_PARALLEL_TILES
#include "carve_tiles.glsl"
#endif

Body carve_build_cached(Body source,uint child,uint pointer,uint generation) {
    Body b=source;b.piece_count=PIXEL_BODY_FLAG|PF_COUNT(child);b.compound_reserved=pointer;
    b.material=PF_MATERIAL(child);b.motion_flags=source.reserved0==0x53545231u?0u:source.motion_flags;
    b.rest_ticks=0u;b.reserved0=0x53545232u;b.reserved1=generation;
    b.mass=intBitsToFloat(PF_HIGH(child).x);b.center=intBitsToFloat(PF_LOW(child));
    b.inertia=intBitsToFloat(PF_HIGH(child).y);
    b.density=b.mass/float(PF_COUNT(child));b.motion_ready=1u;
    b.inverse_mass=(b.motion_flags&1u)==0u?1./b.mass:0.;
    b.inverse_inertia=(b.motion_flags&3u)==0u?1./b.inertia:0.;
    vec2 lo=pixel_low(b),hi=pixel_high(b);
    b.radius=max(max(length(lo-b.center),length(vec2(hi.x,lo.y)-b.center)),
                 max(length(vec2(lo.x,hi.y)-b.center),length(hi-b.center)));
    if(!fragment_pose(source,b)) b.enabled=0u;
    return b;
}

bool pixel_carve_parallel(uint slot,Body source,uint generation) {
    uint lane=gl_LocalInvocationIndex,area=body_pieces(source);
    if(lane==0u) {
        pc_failed=0u;pc_available=1u;PF_SLOT(0)=slot;PF_ID(0)=source.id;pc_count=0u;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(i!=slot&&DC_BODY_FREE(i)) PF_SLOT(pc_available++)=i;
    }
    memoryBarrierBuffer();barrier();
#if DC_CARVE_UNION_LABELS && DC_CARVE_REUSE_CLASSIFICATION
    if(!carve_union_components(source,area)) return false;
#else
    const ivec2 neighbors[8]=ivec2[8](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1),
        ivec2(-1,-1),ivec2(1,-1),ivec2(-1,1),ivec2(1,1));
    for(;;) {
        if(lane==0u) pc_seed=area;
        barrier();
        for(uint i=lane;i<area;i+=DC_STRUCTURAL_LANES)
            if(DC_FRAGMENT_LABEL(i)==0u&&carve_component_solid(source,i)) atomicMin(pc_seed,i);
        barrier();
        if(pc_seed==area) break;
        if(pc_count==pc_available) {
            if(lane==0u) {atomicMax(DC_RUNTIME_REQUEST(2u),BODY_CAPACITY+1u);atomicOr(DC_RUNTIME_REQUEST(10u),1u);}
            return false;
        }
        if(lane==0u) {
            pc_count++;pc_tail=1u;pc_end=1u;
            DC_FRAGMENT_LABEL(pc_seed)=pc_count;DC_FRAGMENT_QUEUE(0)=pc_seed;
            PF_MATERIAL(pc_count-1u)=piece_material(source,pc_seed);
        }
        ivec2 low=ivec2(0x7fffffff),high=ivec2(0);
        memoryBarrierBuffer();barrier();
        uint head=0u;
        while(head<pc_end) {
#ifdef DC_CARVE_WORK_PROFILE
            if(lane==0u) atomicAdd(carve_work_profile[3],1u);
#endif
            uint end=pc_end;
            for(uint i=head+lane;i<end;i+=DC_STRUCTURAL_LANES) {
                uint piece=DC_FRAGMENT_QUEUE(i);
                ivec2 p=pixel_piece_cell(source,piece);
                low=min(low,p);high=max(high,p+1);
                for(uint n=0u;n<8u;n++) {
                    ivec2 q=p+neighbors[n];
                    if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(source.width,source.height)))) continue;
                    uint next=carve_neighbor_piece(source,piece,neighbors[n],q);
                    if(next==0xffffffffu||!carve_component_solid(source,next)) continue;
                    if(atomicCompSwap(DC_FRAGMENT_LABEL(next),0u,pc_count)==0u)
                        DC_FRAGMENT_QUEUE(atomicAdd(pc_tail,1u))=next;
                }
            }
            memoryBarrierBuffer();barrier();
            if(lane==0u) pc_end=pc_tail;
            head=end;barrier();
        }
        pc_lows[lane]=low;pc_highs[lane]=high;barrier();
        if(lane==0u) {
            for(uint i=1u;i<DC_STRUCTURAL_LANES;i++) {low=min(low,pc_lows[i]);high=max(high,pc_highs[i]);}
            PF_LOW(pc_count-1u)=low;PF_HIGH(pc_count-1u)=high;PF_COUNT(pc_count-1u)=pc_tail;
        }
        memoryBarrierBuffer();barrier();
    }
#endif
    if(pc_count==0u) {
        if(lane==0u) {DC_BODY(slot).enabled=0u;DC_BODY(slot).reserved0=0u;}
        if(reactive_enabled()) for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES) {
            uint material=piece_material(source,pixel);
            if(material!=0u&&material!=5u) {
                uint word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
                reactive_shape_erase(word-color_pool_word(),pixel%8u,color_capacity());
            }
        }
        memoryBarrierBuffer();barrier();return true;
    }
    uint pool=graph()+40000u,capacity=DC_DATA(pool+2u),bank=1u-DC_DATA(pool);
    uint directory=pixel_directory_words(source);
    if(lane==0u) {
        pc_used=0u;pc_total=0u;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(i!=slot&&pixel_body(DC_BODY(i))&&
            (DC_BODY(i).enabled!=0u||DC_BODY(i).reserved0!=0u)) pc_used+=pixel_shape_words(DC_BODY(i));
    }
#if DC_CARVE_PARALLEL_TILES
    carve_count_tiles(source);
#else
    for(uint child=lane;child<pc_count;child+=DC_STRUCTURAL_LANES) {
        uint tiles=0u;
        for(uint tile=0u;tile<pixel_tile_count(source);tile++) {
            bool occupied=false;
            for(uint cell=0u;cell<256u;cell++) if(DC_FRAGMENT_LABEL(tile*256u+cell)==child+1u) {occupied=true;break;}
            tiles+=uint(occupied);
        }
        PF_TILES(child)=tiles;
    }
#endif
    memoryBarrierBuffer();barrier();
    if(lane==0u) for(uint child=0u;child<pc_count;child++) {
        PF_OFFSET(child)=pc_total;pc_total+=8u+directory+PF_TILES(child)*33u;
    }
    memoryBarrierBuffer();barrier();
    if(pc_used>capacity||pc_total>capacity-pc_used) {
        if(lane==0u) {atomicMax(DC_RUNTIME_REQUEST(11u),pc_used+pc_total);atomicOr(DC_RUNTIME_REQUEST(10u),1u);}
        return false;
    }
    if(lane==0u) {
        pc_next=DC_DATA(base()+14u);
        for(uint child=1u;child<pc_count;child++) {
            bool unique=false;
            for(uint attempt=0u;attempt<=BODY_CAPACITY&&!unique;attempt++) {
                if(pc_next==0x7fffffffu) {pc_failed=1u;break;}
                PF_ID(child)=0x80000000u+(++pc_next);unique=true;
                for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_BODY(i).id==PF_ID(child)&&
                    (DC_BODY(i).enabled!=0u||DC_BODY(i).reserved0!=0u)) unique=false;
            }
            if(!unique) pc_failed=1u;
        }
    }
    memoryBarrierBuffer();barrier();if(pc_failed!=0u) return false;
    uint target=pool+16u+bank*capacity,other_used=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        Body b=DC_BODY(i);if(i==slot||!pixel_body(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
        uint length=pixel_shape_words(b);
        for(uint word=lane;word<length;word+=DC_STRUCTURAL_LANES) {
            DC_DATA(target+other_used+word)=pixel_word_at(b.compound_reserved+word);
            shape_color_stage(BODY_STORAGE_WORDS+target+other_used+word,b.compound_reserved+word);
        }
        other_used+=length;
    }
    for(uint child=0u;child<pc_count;child++) {
        uint pointer=target+pc_used+PF_OFFSET(child),length=8u+directory+PF_TILES(child)*33u;
        for(uint word=lane;word<length;word+=DC_STRUCTURAL_LANES) {
            DC_DATA(pointer+word)=0u;shape_color_stage(BODY_STORAGE_WORDS+pointer+word,0u);
        }
        memoryBarrierBuffer();barrier();
        if(lane==0u) {
            for(uint word=0u;word<8u;word++) DC_DATA(pointer+word)=pixel_word_at(source.compound_reserved+word);
            DC_DATA(pointer)=length;DC_DATA(pointer+1u)=PF_TILES(child);
            DC_DATA(pointer+4u)=uint(PF_LOW(child).x);DC_DATA(pointer+5u)=uint(PF_LOW(child).y);
            DC_DATA(pointer+6u)=uint(PF_HIGH(child).x);DC_DATA(pointer+7u)=uint(PF_HIGH(child).y);
        }
#if DC_CARVE_PARALLEL_TILES
        carve_pack_tiles(source,child,pointer,directory);
#else
        uint tile_output=0u;
        for(uint tile=0u;tile<pixel_tile_count(source);tile++) {
#ifdef DC_CARVE_WORK_PROFILE
            if(lane==0u) atomicAdd(carve_work_profile[4],2u);
#endif
            if(lane==0u) pc_seed=0u;barrier();
            for(uint cell=lane;cell<256u;cell+=DC_STRUCTURAL_LANES)
                if(DC_FRAGMENT_LABEL(tile*256u+cell)==child+1u) atomicOr(pc_seed,1u);
            barrier();if(pc_seed==0u) continue;
            uint offset=8u+directory+tile_output++*33u;
            if(lane==0u) {
                uint coord=pixel_word_at(source.compound_reserved+pixel_tile_offset(source,tile));
                DC_DATA(pointer+offset)=coord;
                DC_DATA(pointer+8u+(coord>>16u)*pixel_word_at(source.compound_reserved+2u)+(coord&65535u))=offset;
            }
            if(lane<32u) {
                uint packed=0u;
                for(uint nibble=0u;nibble<8u;nibble++) {
                    uint index=tile*256u+lane*8u+nibble;
                    if(DC_FRAGMENT_LABEL(index)==child+1u) packed|=piece_material(source,index)<<(nibble*4u);
                }
                DC_DATA(pointer+offset+1u+lane)=packed;
                shape_color_stage(BODY_STORAGE_WORDS+pointer+offset+1u+lane,
                    source.compound_reserved+pixel_tile_offset(source,tile)+1u+lane);
            }
        }
#endif
        memoryBarrierBuffer();barrier();
    }
    for(uint child=0u;child<pc_count;child++) {
        Body geometry=source;geometry.compound_reserved=BODY_STORAGE_WORDS+target+pc_used+PF_OFFSET(child);
        geometry.piece_count=PIXEL_BODY_FLAG|PF_COUNT(child);
        uint mass100;vec2 center;float inertia;
        pixel_mass_properties(geometry,mass100,center,inertia);
        if(lane==0u) {
            /* Bounds are already in the output shape header; reuse their
             * scratch fields so validation/publication need no second scan. */
            PF_LOW(child)=floatBitsToInt(center);
            PF_HIGH(child)=floatBitsToInt(vec2(float(mass100)*.01,max(inertia,1e-12)));
            Body b=carve_build_cached(source,child,geometry.compound_reserved,generation);
            if(b.enabled==0u) pc_failed=1u;
        }
        memoryBarrierBuffer();barrier();
    }
    barrier();if(pc_failed!=0u) return false;
    if(lane==0u) {
        other_used=0u;
        for(uint i=0u;i<BODY_CAPACITY;i++) {
            Body b=DC_BODY(i);if(i==slot||!pixel_body(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
            uint length=pixel_shape_words(b);b.compound_reserved=BODY_STORAGE_WORDS+target+other_used;
            other_used+=length;DC_BODY(i)=b;
        }
    }
    memoryBarrierBuffer();barrier();
    uint record=body_fragment_record(slot),root=source.id,root_generation=generation;
    if(DC_SOLVER_DATA(record)!=0u) {root=DC_SOLVER_DATA(record);root_generation=DC_SOLVER_DATA(record+1u);}
    for(uint child=0u;child<pc_count;child++) {
        if(lane==0u) {
            Body b=carve_build_cached(source,child,BODY_STORAGE_WORDS+target+pc_used+PF_OFFSET(child),generation);
            b.id=PF_ID(child);DC_BODY(PF_SLOT(child))=b;
            record=body_fragment_record(PF_SLOT(child));
            DC_SOLVER_DATA(record)=root;DC_SOLVER_DATA(record+1u)=root_generation;
            DC_SOLVER_DATA(record+2u)=pc_next;DC_SOLVER_DATA(record+3u)=1u;
        }
    }
    if(lane==0u) {
        DC_DATA(pool)=bank;DC_DATA(pool+1u)=pc_used+pc_total;DC_DATA(base()+14u)=pc_next;
        DC_DATA(base()+16u)=(params.width+15u)/16u;DC_DATA(base()+17u)=(params.height+15u)/16u;DC_DATA(base()+18u)=1u;
    }
    memoryBarrierBuffer();barrier();
    if(reactive_enabled()) for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES) {
        uint material=piece_material(source,pixel);if(material==0u) continue;
        uint word=source.compound_reserved+pixel_tile_offset(source,pixel/256u)+1u+(pixel%256u)/8u;
        uint label=DC_FRAGMENT_LABEL(pixel);
        if(label>0u&&label<=pc_count)
            reactive_shape_reown(word-color_pool_word(),pixel%8u,color_capacity(),PF_ID(label-1u),generation,uvec2(pixel_piece_cell(source,pixel)));
        else if(material!=5u) reactive_shape_erase(word-color_pool_word(),pixel%8u,color_capacity());
    }
    memoryBarrierBuffer();barrier();return true;
}
