/* Exact cell-bond cuts. Scratch labels/queue are GPU-only and reused after
 * component extraction. The alternate arena bank is staging until conservation
 * and all output reservations have succeeded. */
Body pixel_fragment_build(Body source,uint count,uint material,uint pointer,uint generation) {
    Body b=source;b.piece_count=PIXEL_BODY_FLAG|count;b.compound_reserved=pointer;
    b.material=material;b.motion_ready=0u;b.motion_flags=0u;b.rest_ticks=0u;
    b.reserved0=0x53545232u;b.reserved1=generation;body_properties(b);
    vec2 offset=body_rotate(b.center-source.center,source.angle);
    vec2 velocity=vec2(source.vx_fp,source.vy_fp)/65536.0+
        source.angular_velocity*vec2(-offset.y,offset.x);
    b.vx_fp=int(round(velocity.x*65536.0));b.vy_fp=int(round(velocity.y*65536.0));
    body_cache_bounds(b);return b;
}
bool pixel_fragment_body(uint slot,uint axis,float cut,uint generation) {
    Body source=bodies[slot];uint area=source.width*source.height;
    if(area>max(params.width*params.height,data[base()+1u]*4096u)) return false;
    uint slots[64],ids[64],counts[64],materials[64];
    uint available=1u;slots[0]=slot;ids[0]=source.id;
    for(uint i=0u;i<BODY_CAPACITY;i++)
        if(i!=slot&&bodies[i].enabled==0u&&bodies[i].reserved0==0u) slots[available++]=i;
    for(uint i=0u;i<area;i++) fracture_labels[i]=0u;
    uint count=0u;int plane=int(round(cut));
    const ivec2 neighbors[4]=ivec2[4](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1));
    for(uint seed=0u;seed<area;seed++) {
        uint material=piece_material(source,seed);
        if(material==0u||fracture_labels[seed]!=0u) continue;
        if(count==available) return false;
        uint owner=++count,head=0u,tail=1u;
        materials[owner-1u]=material;fracture_labels[seed]=owner;fracture_queue[0]=seed;
        while(head<tail) {
            uint cell=fracture_queue[head++];ivec2 p=ivec2(cell%source.width,cell/source.width);
            for(uint n=0u;n<4u;n++) {
                ivec2 q=p+neighbors[n];
                if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(source.width,source.height)))||
                   (p[axis]<plane)!=(q[axis]<plane)) continue;
                uint next=uint(q.y)*source.width+uint(q.x);
                if(fracture_labels[next]!=0u||pixel_material(source,q)==0u) continue;
                fracture_labels[next]=owner;fracture_queue[tail++]=next;
            }
        }
        counts[owner-1u]=tail;
    }
    if(count<2u) return false;
    uint pool=graph()+40000u,capacity=data[pool+2u],bank=1u-data[pool],used=0u;
    uint words=(area+7u)/8u;
    for(uint i=0u;i<BODY_CAPACITY;i++) if(i!=slot&&pixel_body(bodies[i])&&
        (bodies[i].enabled!=0u||bodies[i].reserved0!=0u))
        used+=(bodies[i].width*bodies[i].height+7u)/8u;
    if(used>capacity||count> (capacity-used)/words) return false;
    uint next=data[base()+14u];
    for(uint child=1u;child<count;child++) {
        bool unique=false;
        for(uint attempt=0u;attempt<=BODY_CAPACITY&&!unique;attempt++) {
            if(next==0x7fffffffu) return false;
            ids[child]=0x80000000u+(++next);unique=true;
            for(uint i=0u;i<BODY_CAPACITY;i++)
                if(bodies[i].id==ids[child]&&(bodies[i].enabled!=0u||bodies[i].reserved0!=0u)) unique=false;
        }
        if(!unique) return false;
    }
    uint target=pool+16u+bank*capacity,other_used=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        Body b=bodies[i];if(i==slot||!pixel_body(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
        uint length=(b.width*b.height+7u)/8u;
        for(uint word=0u;word<length;word++) data[target+other_used+word]=pixel_word_at(b.compound_reserved+word);
        other_used+=length;
    }
    for(uint child=0u;child<count;child++) {
        uint pointer=target+used+child*words;
        for(uint word=0u;word<words;word++) {
            uint packed=0u;
            for(uint nibble=0u;nibble<8u;nibble++) {
                uint index=word*8u+nibble;
                if(index<area&&fracture_labels[index]==child+1u)
                    packed|=piece_material(source,index)<<(nibble*4u);
            }
            data[pointer+word]=packed;
        }
    }
    float mass=0,inertia=0,angular=0,energy=0;vec2 momentum=vec2(0);
    for(uint child=0u;child<count;child++) {
        Body b=pixel_fragment_build(source,counts[child],materials[child],
            BODY_STORAGE_WORDS+target+used+child*words,generation);
        vec2 r=body_rotate(b.center-source.center,source.angle),v=vec2(b.vx_fp,b.vy_fp)/65536.;
        mass+=b.mass;inertia+=b.inertia+b.mass*dot(r,r);momentum+=b.mass*v;
        angular+=b.inertia*b.angular_velocity+body_cross(r,b.mass*v);
        energy+=.5*b.mass*dot(v,v)+.5*b.inertia*b.angular_velocity*b.angular_velocity;
    }
    vec2 velocity=vec2(source.vx_fp,source.vy_fp)/65536.;
    float initial=.5*source.mass*dot(velocity,velocity)+.5*source.inertia*source.angular_velocity*source.angular_velocity;
    if(abs(mass-source.mass)>max(.002,source.mass*1e-5)||
       abs(inertia-source.inertia)>max(.002,source.inertia*1e-4)||
       length(momentum-source.mass*velocity)>max(.002,source.mass*length(velocity)*1e-4)||
       abs(angular-source.inertia*source.angular_velocity)>max(.002,abs(source.inertia*source.angular_velocity)*1e-4)||
       energy>initial+max(.002,initial*1e-4)) return false;
    other_used=0u;
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        Body b=bodies[i];if(i==slot||!pixel_body(b)||(b.enabled==0u&&b.reserved0==0u)) continue;
        b.compound_reserved=BODY_STORAGE_WORDS+target+other_used;
        other_used+=(b.width*b.height+7u)/8u;bodies[i]=b;
    }
    uint root=source.id,root_generation=generation,record=graph()+36000u+slot*4u;
    if(data[record]!=0u) {root=data[record];root_generation=data[record+1u];}
    for(uint child=0u;child<count;child++) {
        Body b=pixel_fragment_build(source,counts[child],materials[child],
            BODY_STORAGE_WORDS+target+used+child*words,generation);
        b.id=ids[child];bodies[slots[child]]=b;
        record=graph()+36000u+slots[child]*4u;
        data[record]=root;data[record+1u]=root_generation;data[record+2u]=next;data[record+3u]=1u;
    }
    data[pool]=bank;data[pool+1u]=used+count*words;data[base()+14u]=next;
    data[base()+16u]=(params.width+15u)/16u;data[base()+17u]=(params.height+15u)/16u;data[base()+18u]=1u;
    return true;
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
            float t=uintBitsToFloat(data[profile+6u]);
            if((material==2u||material==4u)&&axis==1u) t*=uintBitsToFloat(data[profile+9u]);
            toughness=min(toughness,t);
        }
        work+=toughness;
    }
    return work;
}
