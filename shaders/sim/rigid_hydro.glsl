layout(set=0,binding=1,std430) readonly buffer HydroCells { uvec2 hydro_cells[]; };
layout(set=0,binding=3,std430) readonly buffer HydroOccupancy { uint hydro_occupancy[]; };
layout(set=0,binding=10,std430) readonly buffer HydroVelocity { vec2 hydro_velocity[]; };
#include "rigid_clip.glsl"

/* Grid-volume viscous relaxation per tick. The lowest material density is .6,
   so even complete immersion damps relative velocity by at most 1/20 per tick. */
const float WATER_DRAG_RATE=.03;

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

bool hydro_near_water(vec2 low,vec2 high) {
    ivec2 begin=max(ivec2(floor(low))-1,ivec2(0));
    ivec2 end=min(ivec2(ceil(high))+1,ivec2(params.width,params.height));
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 p=ivec2(x,y);uint page=hydro_page(p);
        if(page!=0u&&hydro_cells[hydro_atlas(p,page)].y!=0u) return true;
    }
    return false;
}

bool hydro_pool_column(int x,int begin_y,out float level) {
    int floor_y=-1;level=0;
    for(int y=max(begin_y,0);y<int(params.height);y++) {
        ivec2 p=ivec2(x,y);uint page=hydro_page(p);
        if(page==0u) return false;
        if(!hydro_permeable(hydro_cells[hydro_atlas(p,page)].x&0xffffu)) {floor_y=y;break;}
    }
    if(floor_y<=0) return false;
    ivec2 bottom=ivec2(x,floor_y-1);uint bottom_page=hydro_page(bottom);
    if(bottom_page==0u||hydro_cells[hydro_atlas(bottom,bottom_page)].y==0u) return false;
    float volume=0;
    for(int y=floor_y-1;y>=0;y--) {
        ivec2 p=ivec2(x,y);uint page=hydro_page(p);
        if(page==0u) break;
        uint atlas=hydro_atlas(p,page);
        if(!hydro_permeable(hydro_cells[atlas].x&0xffffu)) break;
        if(hydro_occupancy[uint(y)*params.width+uint(x)]!=0u) return false;
        volume+=float(hydro_cells[atlas].y)/65536.0;
    }
    level=float(floor_y)-volume;
    return true;
}

bool hydro_pool_surface(vec2 low,vec2 high,out float level) {
    /* Hydrostatic reconstruction uses clear columns outside the whole hull.
       It excludes the transient water films produced by geometric rezoning. */
    int y=max(int(floor(low.y)),0);float total=0;uint count=0u;
    for(int side=-1;side<=1;side+=2) {
        int edge=side<0?int(floor(low.x)):int(ceil(high.x))-1;
        for(int distance=1;distance<=16;distance++) {
            int x=edge+side*distance;ivec2 p=ivec2(x,y);uint page=hydro_page(p);
            if(page==0u||!hydro_permeable(hydro_cells[hydro_atlas(p,page)].x&0xffffu)) break;
            float sample_level;
            if(hydro_pool_column(x,y,sample_level)) {total+=sample_level;count++;}
        }
    }
    level=total/max(float(count),1);
    return count!=0u;
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

void hydro_impulse(uint slot,inout Body b) {
    vec2 low,high;body_bounds(b,low,high);
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0,center=origin+b.center;
    if(!hydro_near_water(origin+low,origin+high)) return;
    float waterline;bool pooled=hydro_pool_surface(origin+low,origin+high,waterline);
    ivec2 begin=max(ivec2(floor(origin+low)),ivec2(0));
    ivec2 end=min(ivec2(ceil(origin+high)),ivec2(params.width,params.height));
    vec2 force=vec2(0),v=vec2(b.vx_fp,b.vy_fp)/65536.0;float torque=0;
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 cell=ivec2(x,y);vec2 flow;float fraction=hydro_sample(cell,flow);
        if(pooled) fraction=clamp(float(y)+1-waterline,0,1);
        if(fraction<=0) continue;
        for(uint piece=0u;piece<body_pieces(b);piece++) {
            vec2 point;float volume=body_cell_volume(b,piece,cell,fraction,point);
            if(volume<=0) continue;
            vec2 r=point-center,relative=flow-v-b.angular_velocity*vec2(-r.y,r.x);
            vec2 impulse=volume*(vec2(0,-.25)+WATER_DRAG_RATE*relative);
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
