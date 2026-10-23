const uint FRAGMENT_CAPACITY=8u,FRAGMENT_ID_BASE=0x80000000u;
void fragment_sum(inout float sum,inout float correction,float value) {
    precise float term=value-correction;
    precise float next=sum+term;
    correction=(next-sum)-term;sum=next;
}
bool fragment_clip(Body source,uint piece,uint axis,float cut,bool lower,out Shape shape) {
    shape.count=0u;shape.material=source.piece_count==0u ? source.material : source.pieces[piece].material;
    for(uint i=0u;i<BODY_VERTEX_CAPACITY;i++) shape.vertices[i]=ivec2(0);
    for(uint i=0u;i<piece_vertices(source,piece);i++) {
        vec2 a=piece_rest_vertex(source,piece,i);
        vec2 b=piece_rest_vertex(source,piece,(i+1u)%piece_vertices(source,piece));
        float da=lower?cut-a[axis]:a[axis]-cut,db=lower?cut-b[axis]:b[axis]-cut;
        if(da>=0) {
            if(shape.count==BODY_VERTEX_CAPACITY) return false;
            shape.vertices[shape.count++]=ivec2(round(a*65536.0));
        }
        if((da>0&&db<0)||(da<0&&db>0)) {
            if(shape.count==BODY_VERTEX_CAPACITY) return false;
            shape.vertices[shape.count++]=ivec2(round(mix(a,b,da/(da-db))*65536.0));
        }
    }
    float area=0;
    if(shape.count>=3u) for(uint i=1u;i+1u<shape.count;i++)
        area+=body_cross(vec2(shape.vertices[i]-shape.vertices[0])/65536.0,
                         vec2(shape.vertices[i+1u]-shape.vertices[0])/65536.0)*.5;
    if(area<1e-7) shape.count=0u;
    return true;
}
bool fragment_connected(Shape a,Shape b) {
    for(uint i=0u;i<a.count;i++) for(uint j=0u;j<b.count;j++) {
        vec2 p=vec2(a.vertices[i])/65536.0;
        vec2 d=vec2(a.vertices[(i+1u)%a.count]-a.vertices[i])/65536.0;
        float edge_length=length(d);if(edge_length<1e-7) continue;
        vec2 n=d/edge_length;
        vec2 q=vec2(b.vertices[j])/65536.0-p;
        vec2 z=vec2(b.vertices[(j+1u)%b.count])/65536.0-p;
        if(abs(body_cross(n,q))>2.0/65536.0||abs(body_cross(n,z))>2.0/65536.0) continue;
        float lo=min(dot(n,q),dot(n,z)),hi=max(dot(n,q),dot(n,z));
        if(min(edge_length,hi)-max(0,lo)>2.0/65536.0) return true;
    }
    return false;
}
bool fragment_pose(Body source,inout Body b) {
    vec2 delta=b.center-source.center;
    ivec2 shift=ivec2(round((body_rotate(delta,source.angle)-delta)*65536.));
    ivec2 local=source.local_fp+shift;uvec2 x,y;
    if(!shifted_chunk(source.chunk_x,local.x>>22,x)||!shifted_chunk(source.chunk_y,local.y>>22,y)) return false;
    b.chunk_x=x;b.chunk_y=y;b.local_fp=local&ivec2((64<<16)-1);
    b.x_fp=source.x_fp+shift.x;b.y_fp=source.y_fp+shift.y;
    ivec2 previous=ivec2(round((body_rotate(delta,source.previous_angle)-delta)*65536.));
    b.previous_x_fp=source.previous_x_fp+previous.x;b.previous_y_fp=source.previous_y_fp+previous.y;
    vec2 offset=vec2(shift)/65536.+delta;
    vec2 v=vec2(source.vx_fp,source.vy_fp)/65536.+source.angular_velocity*vec2(-offset.y,offset.x);
    b.vx_fp=int(round(v.x*65536.));b.vy_fp=int(round(v.y*65536.));
    body_cache_bounds(b);return true;
}
Body fragment_build(Body source,Shape pieces[8],uint labels[8],uint owner,uint generation) {
    Body b=source;b.piece_count=0u;
    for(uint i=0u;i<FRAGMENT_CAPACITY;i++) if(pieces[i].count!=0u&&labels[i]==owner)
        b.pieces[b.piece_count++]=pieces[i];
    b.material=b.pieces[0].material;
    if(b.piece_count==1u) {
        b.vertex_count=b.pieces[0].count;
        for(uint i=0u;i<b.vertex_count;i++) b.vertices[i]=b.pieces[0].vertices[i];
        b.piece_count=0u;
    } else b.vertex_count=0u;
    b.motion_ready=0u;b.motion_flags=0u;b.rest_ticks=0u;
    b.reserved0=0x53545232u;b.reserved1=generation;
    body_properties(b);
    if(!fragment_pose(source,b)) b.enabled=0u;
    return b;
}
bool fragment_body(uint slot,uint axis,float cut,uint generation) {
    Body source=DC_BODY(slot);Shape pieces[8];uint labels[8],owners[8],slots[8],ids[8];
    if(source.material==0u) source.material=1u;
    for(uint i=0u;i<FRAGMENT_CAPACITY;i++) {pieces[i].count=0u;labels[i]=i;}
    cut=round(cut*65536.0)/65536.0;
    for(uint section=0u;section<2u;section++) for(uint p=0u;p<body_pieces(source);p++)
        if(!fragment_clip(source,p,axis,cut,section==0u,pieces[section*4u+p])) return false;
    for(uint section=0u;section<2u;section++) for(uint pass=0u;pass<4u;pass++)
        for(uint a=section*4u;a<section*4u+4u;a++) for(uint b=a+1u;b<section*4u+4u;b++)
            if(pieces[a].count!=0u&&pieces[b].count!=0u&&fragment_connected(pieces[a],pieces[b])) {
                uint la=labels[a],lb=labels[b],label=min(la,lb);
                for(uint i=section*4u;i<section*4u+4u;i++) if(labels[i]==la||labels[i]==lb) labels[i]=label;
            }
    uint count=0u;
    for(uint i=0u;i<FRAGMENT_CAPACITY;i++) if(pieces[i].count!=0u&&labels[i]==i) owners[count++]=i;
    if(count<2u) return false;
    slots[0]=slot;ids[0]=source.id;uint allocated=1u;
    for(uint i=0u;i<BODY_CAPACITY&&allocated<count;i++)
        if(i!=slot&&DC_BODY_FREE(i)) slots[allocated++]=i;
    if(allocated!=count) {
#ifdef DC_RUNTIME_BODY_STORAGE
        atomicMax(DC_RUNTIME_REQUEST(2u),BODY_CAPACITY+count-allocated);
#endif
        return false;
    }
    uint base=structural_base(),next=DC_DATA(base+14u);
    for(uint child=1u;child<count;child++) {
        bool available=false;
        for(uint attempt=0u;attempt<=BODY_CAPACITY&&!available;attempt++) {
            if(next==0x7fffffffu) return false;
            ids[child]=FRAGMENT_ID_BASE+(++next);available=true;
            for(uint i=0u;i<BODY_CAPACITY;i++)
                if(DC_BODY(i).id==ids[child]&&(DC_BODY(i).enabled!=0u||DC_BODY(i).reserved0!=0u)) available=false;
        }
        if(!available) return false;
    }
    float mass=0,inertia=0,energy=0;vec2 momentum=vec2(0);float angular=0;
    for(uint child=0u;child<count;child++) {
        Body b=fragment_build(source,pieces,labels,owners[child],generation);
        if(b.enabled==0u) return false;
        vec2 r=vec2(b.x_fp-source.x_fp,b.y_fp-source.y_fp)/65536.+b.center-source.center;
        vec2 v=vec2(b.vx_fp,b.vy_fp)/65536.0;
        mass+=b.mass;inertia+=b.inertia+b.mass*dot(r,r);momentum+=b.mass*v;
        angular+=b.inertia*b.angular_velocity+body_cross(r,b.mass*v);
        energy+=.5*b.mass*dot(v,v)+.5*b.inertia*b.angular_velocity*b.angular_velocity;
    }
    vec2 v=vec2(source.vx_fp,source.vy_fp)/65536.0;
    float initial=.5*source.mass*dot(v,v)+.5*source.inertia*source.angular_velocity*source.angular_velocity;
    if(abs(mass-source.mass)>max(1e-4,source.mass*1e-5)||
       abs(inertia-source.inertia)>max(1e-3,source.inertia*1e-4)||
       length(momentum-source.mass*v)>max(.002,source.mass*length(v)*1e-4)||
       abs(angular-source.inertia*source.angular_velocity)>max(.002,abs(source.inertia*source.angular_velocity)*1e-4)||
       energy>initial+max(.002,initial*1e-4)) return false;
    uint root=source.id,root_generation=generation,g=structural_graph();
    if(DC_SOLVER_DATA(body_fragment_record(slot))!=0u) {
        root=DC_SOLVER_DATA(body_fragment_record(slot));root_generation=DC_SOLVER_DATA(body_fragment_record(slot)+1u);
    }
    for(uint child=0u;child<count;child++) {
        Body b=fragment_build(source,pieces,labels,owners[child],generation);
        b.id=ids[child];DC_BODY(slots[child])=b;
        uint record=body_fragment_record(slots[child]);
        DC_SOLVER_DATA(record)=root;DC_SOLVER_DATA(record+1u)=root_generation;DC_SOLVER_DATA(record+2u)=next;DC_SOLVER_DATA(record+3u)=1u;
    }
    DC_DATA(base+14u)=next;
    DC_DATA(base+16u)=(params.width+15u)/16u;
    DC_DATA(base+17u)=(params.height+15u)/16u;DC_DATA(base+18u)=1u;
    return true;
}
