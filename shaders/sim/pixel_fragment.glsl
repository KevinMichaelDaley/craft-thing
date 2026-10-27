#ifdef DC_RUNTIME_BODY_STORAGE
#define PF_SLOT(i) DC_FRAGMENT_WORD(0u,i)
#define PF_ID(i) DC_FRAGMENT_WORD(1u,i)
#define PF_COUNT(i) DC_FRAGMENT_WORD(2u,i)
#define PF_MATERIAL(i) DC_FRAGMENT_WORD(3u,i)
#define PF_TILES(i) DC_FRAGMENT_WORD(4u,i)
#define PF_OFFSET(i) DC_FRAGMENT_WORD(5u,i)
#define PF_LOW(i) DC_FRAGMENT_POINT(6u,i)
#define PF_HIGH(i) DC_FRAGMENT_POINT(8u,i)
#else
#define PF_SLOT(i) slots[i]
#define PF_ID(i) ids[i]
#define PF_COUNT(i) counts[i]
#define PF_MATERIAL(i) materials[i]
#define PF_TILES(i) tile_counts[i]
#define PF_OFFSET(i) offsets[i]
#define PF_LOW(i) lows[i]
#define PF_HIGH(i) highs[i]
#endif
/* Exact cell-bond cuts. Scratch labels/queue are GPU-only and reused after
 * component extraction. The alternate arena bank is staging until conservation
 * and all output reservations have succeeded. */
Body pixel_fragment_build(Body source,uint count,uint material,uint pointer,uint generation) {
    Body b=source;b.piece_count=PIXEL_BODY_FLAG|count;b.compound_reserved=pointer;
    b.material=material;b.motion_ready=0u;b.motion_flags=0u;b.rest_ticks=0u;
    b.reserved0=0x53545232u;b.reserved1=generation;body_properties(b);
    if(!fragment_pose(source,b)) b.enabled=0u;
    return b;
}
bool carve_pixel(Body source,uint pixel) {
    vec2 rest=vec2(pixel_piece_cell(source,pixel))+.5;
    vec2 world=vec2(source.x_fp,source.y_fp)/65536.+source.center+body_rotate(rest-source.center,source.angle);
    vec2 delta=floor(world)-vec2(params.origin_x);
    return dot(delta,delta)<=float(params.origin_y.x)*float(params.origin_y.x);
}
bool pixel_fragment_source(uint slot,Body source,uint axis,float cut,uint generation) {
    uint area=body_pieces(source);bool carving=axis==2u;
#ifdef DC_RUNTIME_BODY_STORAGE
    if(area>DC_RUNTIME_WORK(13u)) {
        atomicMax(DC_RUNTIME_REQUEST(9u),area);
        if(carving&&STRUCTURAL_PHASE==6u) atomicOr(DC_RUNTIME_REQUEST(10u),1u);
        return false;
    }
#endif
#ifndef DC_RUNTIME_BODY_STORAGE
    uint slots[64],ids[64],counts[64],materials[64],tile_counts[64],offsets[64];
    ivec2 lows[64],highs[64];
#endif
    uint available=1u;PF_SLOT(0)=slot;PF_ID(0)=source.id;
    for(uint i=0u;i<BODY_CAPACITY;i++)
        if(i!=slot&&DC_BODY_FREE(i)) PF_SLOT(available++)=i;
    if(!carving) for(uint i=0u;i<area;i++) DC_FRAGMENT_LABEL(i)=0u;
    uint count=0u;int plane=int(round(cut));
    const ivec2 neighbors[8]=ivec2[8](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1),
        ivec2(-1,-1),ivec2(1,-1),ivec2(-1,1),ivec2(1,1));
    for(uint seed=0u;seed<area;seed++) {
        uint material=piece_material(source,seed);
        if(material==0u||DC_FRAGMENT_LABEL(seed)!=0u) continue;
        if(count==available) {
#ifdef DC_RUNTIME_BODY_STORAGE
            atomicMax(DC_RUNTIME_REQUEST(2u),BODY_CAPACITY+1u);
            if(carving&&STRUCTURAL_PHASE==6u) atomicOr(DC_RUNTIME_REQUEST(10u),1u);
#endif
            return false;
        }
        uint owner=++count,head=0u,tail=1u;
        PF_LOW(owner-1u)=pixel_piece_cell(source,seed);PF_HIGH(owner-1u)=PF_LOW(owner-1u)+1;
        PF_MATERIAL(owner-1u)=material;DC_FRAGMENT_LABEL(seed)=owner;DC_FRAGMENT_QUEUE(0)=seed;
        while(head<tail) {
            uint cell=DC_FRAGMENT_QUEUE(head++);ivec2 p=pixel_piece_cell(source,cell);
            PF_LOW(owner-1u)=min(PF_LOW(owner-1u),p);PF_HIGH(owner-1u)=max(PF_HIGH(owner-1u),p+1);
            for(uint n=0u;n<8u;n++) {
                ivec2 q=p+neighbors[n];
                if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(source.width,source.height)))||
                   (!carving&&(p[axis]<plane)!=(q[axis]<plane))) continue;
                uint next=pixel_piece_at(source,q);
                if(next==0xffffffffu||DC_FRAGMENT_LABEL(next)!=0u||piece_material(source,next)==0u) continue;
                DC_FRAGMENT_LABEL(next)=owner;DC_FRAGMENT_QUEUE(tail++)=next;
            }
        }
        PF_COUNT(owner-1u)=tail;
    }
    if(carving&&count==0u) {DC_BODY(slot).enabled=0u;DC_BODY(slot).reserved0=0u;return true;}
    if(!carving&&count<2u) return false;
    uint pool=graph()+40000u,capacity=DC_DATA(pool+2u),bank=1u-DC_DATA(pool),used=0u;
    uint directory=pixel_directory_words(source),total=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) if(i!=slot&&pixel_storage(DC_BODY(i))&&
        (DC_BODY(i).enabled!=0u||DC_BODY(i).reserved0!=0u))
        used+=pixel_shape_words(DC_BODY(i));
    for(uint child=0u;child<count;child++) {
        PF_TILES(child)=0u;
        for(uint tile=0u;tile<pixel_tile_count(source);tile++) {
            bool occupied=false;
            for(uint cell=0u;cell<256u;cell++) if(DC_FRAGMENT_LABEL(tile*256u+cell)==child+1u) {occupied=true;break;}
            PF_TILES(child)+=uint(occupied);
        }
        PF_OFFSET(child)=total;total+=8u+directory+PF_TILES(child)*33u;
    }
    if(used>capacity||total>capacity-used) {
#ifdef DC_RUNTIME_BODY_STORAGE
        atomicMax(DC_RUNTIME_REQUEST(11u),used+total);
        if(carving&&STRUCTURAL_PHASE==6u) atomicOr(DC_RUNTIME_REQUEST(10u),1u);
#endif
        return false;
    }
    uint next=DC_DATA(base()+14u);
    for(uint child=1u;child<count;child++) {
        bool unique=false;
        for(uint attempt=0u;attempt<=BODY_CAPACITY&&!unique;attempt++) {
            if(next==0x7fffffffu) return false;
            PF_ID(child)=0x80000000u+(++next);unique=true;
            for(uint i=0u;i<BODY_CAPACITY;i++)
                if(DC_BODY(i).id==PF_ID(child)&&(DC_BODY(i).enabled!=0u||DC_BODY(i).reserved0!=0u)) unique=false;
        }
        if(!unique) return false;
    }
    uint target=pool+16u+bank*capacity,other_used=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        Body b=DC_BODY(i);if(i==slot||!pixel_storage(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
        uint length=pixel_shape_words(b);
        for(uint word=0u;word<length;word++) {
            DC_DATA(target+other_used+word)=pixel_word_at(b.compound_reserved+word);
            shape_color_stage(BODY_STORAGE_WORDS+target+other_used+word,b.compound_reserved+word);
        }
        other_used+=length;
    }
    for(uint child=0u;child<count;child++) {
        uint pointer=target+used+PF_OFFSET(child),length=8u+directory+PF_TILES(child)*33u;
        for(uint word=0u;word<length;word++) {DC_DATA(pointer+word)=0u;shape_color_stage(BODY_STORAGE_WORDS+pointer+word,0u);}
        for(uint word=0u;word<8u;word++) DC_DATA(pointer+word)=pixel_word_at(source.compound_reserved+word);
        DC_DATA(pointer)=length;DC_DATA(pointer+1u)=PF_TILES(child);
        DC_DATA(pointer+4u)=uint(PF_LOW(child).x);DC_DATA(pointer+5u)=uint(PF_LOW(child).y);
        DC_DATA(pointer+6u)=uint(PF_HIGH(child).x);DC_DATA(pointer+7u)=uint(PF_HIGH(child).y);
        uint tile_output=0u,directory_width=DC_DATA(pointer+2u);
        for(uint tile=0u;tile<pixel_tile_count(source);tile++) {
            bool occupied=false;
            for(uint cell=0u;cell<256u;cell++) if(DC_FRAGMENT_LABEL(tile*256u+cell)==child+1u) {occupied=true;break;}
            if(!occupied) continue;
            uint offset=8u+directory+tile_output++*33u;
            uint coord=pixel_word_at(source.compound_reserved+pixel_tile_offset(source,tile));
            DC_DATA(pointer+offset)=coord;
            DC_DATA(pointer+8u+(coord>>16)*directory_width+(coord&65535u))=offset;
            for(uint word=0u;word<32u;word++) {
                uint packed=0u;
                for(uint nibble=0u;nibble<8u;nibble++) {
                    uint index=tile*256u+word*8u+nibble;
                    if(DC_FRAGMENT_LABEL(index)==child+1u)
                        packed|=piece_material(source,index)<<(nibble*4u);
                }
                DC_DATA(pointer+offset+1u+word)=packed;
                shape_color_stage(BODY_STORAGE_WORDS+pointer+offset+1u+word,
                    source.compound_reserved+pixel_tile_offset(source,tile)+1u+word);
            }
        }
    }
    float mass=0,inertia=0,angular=0,energy=0;vec2 momentum=vec2(0);
    float mass_error=0,inertia_error=0,angular_error=0,energy_error=0;vec2 momentum_error=vec2(0);
    vec2 velocity=vec2(source.vx_fp,source.vy_fp)/65536.;
    for(uint child=0u;child<count;child++) {
        Body b=pixel_fragment_build(source,PF_COUNT(child),PF_MATERIAL(child),
            BODY_STORAGE_WORDS+target+used+PF_OFFSET(child),generation);
        if(b.enabled==0u) return false;
        vec2 r=vec2(b.x_fp-source.x_fp,b.y_fp-source.y_fp)/65536.+b.center-source.center,v=vec2(b.vx_fp,b.vy_fp)/65536.;
        fragment_sum(mass,mass_error,b.mass);
        fragment_sum(inertia,inertia_error,b.inertia+b.mass*dot(r,r));
        fragment_sum(momentum.x,momentum_error.x,b.mass*v.x);
        fragment_sum(momentum.y,momentum_error.y,b.mass*v.y);
        /* Angular momentum is intrinsic to the parent's translating frame.
         * Subtracting translation before the cross product avoids a large
         * cancellation between fragments around a rounded floating-point COM. */
        fragment_sum(angular,angular_error,b.inertia*b.angular_velocity+body_cross(r,b.mass*(v-velocity)));
        fragment_sum(energy,energy_error,.5*b.mass*dot(v,v)+.5*b.inertia*b.angular_velocity*b.angular_velocity);
    }
    float initial=.5*source.mass*dot(velocity,velocity)+.5*source.inertia*source.angular_velocity*source.angular_velocity;
    if(!carving&&(abs(mass-source.mass)>max(.002,source.mass*1e-5)||
       abs(inertia-source.inertia)>max(.002,source.inertia*1e-4)||
       length(momentum-source.mass*velocity)>max(.002,source.mass*length(velocity)*1e-4)||
       abs(angular-source.inertia*source.angular_velocity)>max(.002,abs(source.inertia*source.angular_velocity)*1e-4)||
       energy>initial+max(.002,initial*1e-4))) {
#ifdef DC_RUNTIME_BODY_STORAGE
        DC_RUNTIME_REQUEST(12u)=uint(abs(mass-source.mass)>max(.002,source.mass*1e-5))|
            (uint(abs(inertia-source.inertia)>max(.002,source.inertia*1e-4))<<1u)|
            (uint(length(momentum-source.mass*velocity)>max(.002,source.mass*length(velocity)*1e-4))<<2u)|
            (uint(abs(angular-source.inertia*source.angular_velocity)>max(.002,abs(source.inertia*source.angular_velocity)*1e-4))<<3u)|
            (uint(energy>initial+max(.002,initial*1e-4))<<4u);
        DC_RUNTIME_REQUEST(13u)=count;
        DC_RUNTIME_REQUEST(14u)=floatBitsToUint(mass-source.mass);
        DC_RUNTIME_REQUEST(15u)=floatBitsToUint(inertia-source.inertia);
#endif
        return false;
    }
    other_used=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        Body b=DC_BODY(i);if(i==slot||!pixel_storage(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
        b.compound_reserved=BODY_STORAGE_WORDS+target+other_used;
        other_used+=pixel_shape_words(b);DC_BODY(i)=b;
    }
    uint root=source.id,root_generation=generation,record=body_fragment_record(slot);
    if(DC_SOLVER_DATA(record)!=0u) {root=DC_SOLVER_DATA(record);root_generation=DC_SOLVER_DATA(record+1u);}
    for(uint child=0u;child<count;child++) {
        Body b=pixel_fragment_build(source,PF_COUNT(child),PF_MATERIAL(child),
            BODY_STORAGE_WORDS+target+used+PF_OFFSET(child),generation);
        if(carving&&source.reserved0!=0x53545231u) {b.motion_flags=source.motion_flags;body_properties(b);}
        b.id=PF_ID(child);DC_BODY(PF_SLOT(child))=b;
        record=body_fragment_record(PF_SLOT(child));
        DC_SOLVER_DATA(record)=root;DC_SOLVER_DATA(record+1u)=root_generation;DC_SOLVER_DATA(record+2u)=next;DC_SOLVER_DATA(record+3u)=1u;
    }
    DC_DATA(pool)=bank;DC_DATA(pool+1u)=used+total;DC_DATA(base()+14u)=next;
    DC_DATA(base()+16u)=(params.width+15u)/16u;DC_DATA(base()+17u)=(params.height+15u)/16u;DC_DATA(base()+18u)=1u;
    return true;
}
bool pixel_fragment_body(uint slot,uint axis,float cut,uint generation) {
    return pixel_fragment_source(slot,DC_BODY(slot),axis,cut,generation);
}
float pixel_cut_work(Body b,uint axis,float cut) {
    int plane=int(round(cut));uint span=axis==0u?b.height:b.width;float work=0;
    for(uint i=0u;i<span;i++) {
        ivec2 p=axis==0u?ivec2(plane-1,int(i)):ivec2(int(i),plane-1);
        ivec2 q=p+(axis==0u?ivec2(1,0):ivec2(0,1));
        uint a=pixel_material(b,p),z=pixel_material(b,q);
        if(a==0u||z==0u) continue;
        float toughness=1e20;
        for(uint side=0u;side<2u;side++) {
            uint material=side==0u?a:z;
            uint profile=graph()+29000u+(material==2u||material==4u?1u:material==3u?2u:0u)*12u;
            float t=uintBitsToFloat(DC_DATA(profile+6u));
            if((material==2u||material==4u)&&axis==1u) t*=uintBitsToFloat(DC_DATA(profile+9u));
            toughness=min(toughness,t);
        }
        work+=toughness;
    }
    return work;
}
