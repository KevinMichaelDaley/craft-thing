layout(set=0,binding=1,std430) readonly buffer HydroCells { uvec2 hydro_cells[]; };
layout(set=0,binding=3,std430) readonly buffer HydroOccupancy { uint hydro_occupancy[]; };
layout(set=0,binding=10,std430) readonly buffer HydroVelocity { vec2 hydro_velocity[]; };

uint hydro_page(ivec2 p) {
    if(any(lessThan(p,ivec2(0))) || any(greaterThanEqual(p,ivec2(params.width,params.height)))) return 0u;
    return pages[uint(p.y/64)*((params.width+63u)/64u)+uint(p.x/64)];
}
uint hydro_atlas(ivec2 p,uint page) {
    return (page-1u)*4096u+uint(p.y%64)*64u+uint(p.x%64);
}
bool hydro_permeable(uint material) {
    return material==0u||material==2u||material==4u||material==5u||material==6u;
}
float hydro_sample(ivec2 p,out vec2 flow) {
    flow=vec2(0);uint page=hydro_page(p);
    if(page==0u) return 0;
    uint index=uint(p.y)*params.width+uint(p.x);
    float own=float(hydro_cells[hydro_atlas(p,page)].y)/65536.0;
    if(hydro_occupancy[index]==0u) {flow=hydro_velocity[index];return clamp(own,0,1);}
    float sum=0,weight=0;uint count=0u;
    for(int side=-1;side<=1;side+=2) {
        for(int distance=1;distance<=32;distance++) {
            ivec2 q=p+ivec2(side*distance,0);uint qpage=hydro_page(q);
            if(qpage==0u) break;
            uint atlas=hydro_atlas(q,qpage),qi=uint(q.y)*params.width+uint(q.x);
            if(!hydro_permeable(hydro_cells[atlas].x&0xffffu)) break;
            if(hydro_occupancy[qi]!=0u) continue;
            float fraction=clamp(float(hydro_cells[atlas].y)/65536.0,0,1);
            sum+=fraction;count++;weight+=fraction;flow+=fraction*hydro_velocity[qi];break;
        }
    }
    flow/=max(weight,1e-20);
    return max(clamp(own,0,1),count!=0u?sum/float(count):0);
}

float hydro_clip(Body b,uint piece,ivec2 cell,float fraction,out vec2 center_point) {
    vec2 polygon[12],next[12];uint count=piece_vertices(b,piece);
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0;
    for(uint i=0u;i<count;i++) polygon[i]=origin+piece_vertex(b,piece,i);
    vec2 low=vec2(cell)+vec2(0,1-fraction),high=vec2(cell)+1;
    for(uint plane=0u;plane<4u;plane++) {
        uint axis=plane/2u,n=0u;bool upper=(plane&1u)!=0u;
        float boundary=upper?high[axis]:low[axis];
        for(uint i=0u;i<count;i++) {
            vec2 a=polygon[i],z=polygon[(i+1u)%count];
            float da=upper?boundary-a[axis]:a[axis]-boundary;
            float dz=upper?boundary-z[axis]:z[axis]-boundary;
            if(da>=0) next[n++]=a;
            if((da>0&&dz<0)||(da<0&&dz>0)) next[n++]=mix(a,z,da/(da-dz));
        }
        count=n;for(uint i=0u;i<count;i++) polygon[i]=next[i];
        if(count<3u) {center_point=vec2(0);return 0;}
    }
    float twice=0;vec2 moment=vec2(0);
    for(uint i=0u;i<count;i++) {
        vec2 a=polygon[i]-vec2(cell),z=polygon[(i+1u)%count]-vec2(cell);
        float cross=body_cross(a,z);twice+=cross;moment+=(a+z)*cross;
    }
    center_point=vec2(cell)+moment/max(3*twice,1e-20);
    return max(twice*.5,0);
}

void hydro_impulse(uint slot,inout Body b) {
    vec2 low,high;body_bounds(b,low,high);
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0,center=origin+b.center;
    ivec2 begin=max(ivec2(floor(origin+low)),ivec2(0));
    ivec2 end=min(ivec2(ceil(origin+high)),ivec2(params.width,params.height));
    vec2 force=vec2(0),v=vec2(b.vx_fp,b.vy_fp)/65536.0;float torque=0;
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 cell=ivec2(x,y);vec2 flow;float fraction=hydro_sample(cell,flow);
        if(fraction<=0) continue;
        for(uint piece=0u;piece<body_pieces(b);piece++) {
            vec2 point;float volume=hydro_clip(b,piece,cell,fraction,point);
            if(volume<=0) continue;
            vec2 r=point-center,relative=flow-v-b.angular_velocity*vec2(-r.y,r.x);
            vec2 impulse=volume*(vec2(0,-.25)+.15*relative);
            force+=impulse;torque+=body_cross(r,impulse);
            structural_capture(slot,BODY_CAPACITY,point,impulse);
        }
    }
    if((b.motion_flags&1u)==0u) {
        b.vx_fp+=int(round(force.x*b.inverse_mass*65536.0));
        b.vy_fp+=int(round(force.y*b.inverse_mass*65536.0));
        b.angular_velocity+=torque*b.inverse_inertia;
    }
}
