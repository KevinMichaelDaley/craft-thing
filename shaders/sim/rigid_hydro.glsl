layout(set=0,binding=1,std430) readonly buffer HydroCells { uvec2 hydro_cells[]; };
layout(set=0,binding=3,std430) readonly buffer HydroOccupancy { uint hydro_occupancy[]; };
layout(set=0,binding=10,std430) readonly buffer HydroVelocity { vec2 hydro_velocity[]; };
layout(set=0,binding=37,std430) buffer HydroBulk { uint hydro_bulk[]; };
#include "../../include/dungeoncraft/bulk_phase_layout.h"
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
uint hydro_bulk_word(uint atlas,uint channel) {
    return (atlas/4096u)*FR_BULK_CHUNK_CHANNELS*4096u+
        channel*4096u+atlas%4096u;
}
uint hydro_phase_volume(uint atlas,uint phase) {
    uint stock=hydro_bulk[hydro_bulk_word(atlas,phase-1u)];
    uint retained=hydro_bulk[hydro_bulk_word(atlas,phase+1u)];
    return stock>retained?stock-retained:0u;
}
float hydro_liquid(uint atlas,uint index,out vec2 flow,out float density) {
    float water=float(hydro_cells[atlas].y)/65536.;
    float oil=float(hydro_phase_volume(atlas,1u))/65536.;
    float slime=float(hydro_phase_volume(atlas,2u))/65536.;
    if(oil==0.&&slime==0.) {
        flow=hydro_velocity[index];density=water>0.?1.:0.;
        return clamp(water,0.,1.);
    }
    float volume=water+oil+slime;
    float mass=water+oil*params.oil_density+slime*params.slime_density;
    vec2 momentum=water*hydro_velocity[index];
    uint oil_stock=hydro_bulk[hydro_bulk_word(atlas,0u)];
    uint slime_stock=hydro_bulk[hydro_bulk_word(atlas,1u)];
    float oil_mobile=oil_stock>0u?float(hydro_phase_volume(atlas,1u))/float(oil_stock):0.;
    float slime_mobile=slime_stock>0u?float(hydro_phase_volume(atlas,2u))/float(slime_stock):0.;
    momentum+=oil_mobile*vec2(uintBitsToFloat(hydro_bulk[hydro_bulk_word(atlas,6u)]),
        uintBitsToFloat(hydro_bulk[hydro_bulk_word(atlas,7u)]));
    momentum+=slime_mobile*vec2(uintBitsToFloat(hydro_bulk[hydro_bulk_word(atlas,8u)]),
        uintBitsToFloat(hydro_bulk[hydro_bulk_word(atlas,9u)]));
    flow=mass>0.?momentum/mass:vec2(0.);
    density=volume>0.?mass/volume:0.;
    return clamp(volume,0.,1.);
}
bool hydro_permeable(uint material) {
    return material==0u||material==2u||material==4u||material==5u||material==6u||material==10u;
}

bool hydro_near_liquid(vec2 low,vec2 high) {
    ivec2 begin=max(ivec2(floor(low))-1,ivec2(0));
    ivec2 end=min(ivec2(ceil(high))+1,ivec2(params.width,params.height));
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 p=ivec2(x,y);uint page=hydro_page(p);
        if(page!=0u) {
            uint atlas=hydro_atlas(p,page);
            if(hydro_cells[atlas].y!=0u||hydro_phase_volume(atlas,1u)!=0u||
                hydro_phase_volume(atlas,2u)!=0u) return true;
        }
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
    if(bottom_page==0u) return false;
    uint bottom_at=hydro_atlas(bottom,bottom_page);
    if(hydro_cells[bottom_at].y==0u&&hydro_phase_volume(bottom_at,1u)==0u&&
        hydro_phase_volume(bottom_at,2u)==0u) return false;
    float volume=0;
    for(int y=floor_y-1;y>=0;y--) {
        ivec2 p=ivec2(x,y);uint page=hydro_page(p);
        if(page==0u) break;
        uint atlas=hydro_atlas(p,page);
        if(!hydro_permeable(hydro_cells[atlas].x&0xffffu)) break;
        if(hydro_occupancy[uint(y)*params.width+uint(x)]!=0u) return false;
        volume+=float(hydro_cells[atlas].y+hydro_phase_volume(atlas,1u)+
            hydro_phase_volume(atlas,2u))/65536.0;
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
float hydro_sample(ivec2 p,out vec2 flow,out float density,out uint reaction_at) {
    flow=vec2(0);density=0.;reaction_at=0xffffffffu;
    uint page=hydro_page(p);
    if(page==0u) return 0;
    uint index=uint(p.y)*params.width+uint(p.x);
    uint own_at=hydro_atlas(p,page);
    float own=hydro_liquid(own_at,index,flow,density);
    if(own>0.) reaction_at=own_at;
    if(hydro_occupancy[index]==0u) return own;
    float sum=0,weight=0,density_sum=0.;vec2 neighbor_flow=vec2(0.);uint count=0u;
    for(int side=-1;side<=1;side+=2) {
        for(int distance=1;distance<=32;distance++) {
            ivec2 q=p+ivec2(side*distance,0);uint qpage=hydro_page(q);
            if(qpage==0u) break;
            uint atlas=hydro_atlas(q,qpage),qi=uint(q.y)*params.width+uint(q.x);
            if(!hydro_permeable(hydro_cells[atlas].x&0xffffu)) break;
            if(hydro_occupancy[qi]!=0u) continue;
            vec2 sample_flow;float sample_density;
            float fraction=hydro_liquid(atlas,qi,sample_flow,sample_density);
            sum+=fraction;count++;weight+=fraction;
            density_sum+=fraction*sample_density;
            neighbor_flow+=fraction*sample_flow;
            if(reaction_at==0xffffffffu&&fraction>0.) reaction_at=atlas;
            break;
        }
    }
    if(weight>0.) {
        flow=neighbor_flow/weight;
        if(own<=0.) density=density_sum/weight;
    }
    return max(clamp(own,0,1),count!=0u?sum/float(count):0);
}

void hydro_atomic_momentum(uint word,float delta) {
    uint old=hydro_bulk[word];
    for(;;) {
        uint next=floatBitsToUint(uintBitsToFloat(old)+delta);
        uint observed=atomicCompSwap(hydro_bulk[word],old,next);
        if(observed==old) return;
        old=observed;
    }
}
void hydro_phase_reaction(uint atlas,vec2 impulse) {
    if(atlas==0xffffffffu) return;
    float water=float(hydro_cells[atlas].y)/65536.;
    float oil=float(hydro_phase_volume(atlas,1u))/65536.*params.oil_density;
    float slime=float(hydro_phase_volume(atlas,2u))/65536.*params.slime_density;
    float total=water+oil+slime;
    if(total<=0.) return;
    if(oil>0.) {
        hydro_atomic_momentum(hydro_bulk_word(atlas,6u),-impulse.x*oil/total);
        hydro_atomic_momentum(hydro_bulk_word(atlas,7u),-impulse.y*oil/total);
    }
    if(slime>0.) {
        hydro_atomic_momentum(hydro_bulk_word(atlas,8u),-impulse.x*slime/total);
        hydro_atomic_momentum(hydro_bulk_word(atlas,9u),-impulse.y*slime/total);
    }
}

void hydro_impulse(uint slot,inout Body b) {
    vec2 low,high;body_bounds(b,low,high);
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0,center=origin+b.center;
    if(!hydro_near_liquid(origin+low,origin+high)) return;
    float waterline;bool pooled=hydro_pool_surface(origin+low,origin+high,waterline);
    ivec2 begin=max(ivec2(floor(origin+low)),ivec2(0));
    ivec2 end=min(ivec2(ceil(origin+high)),ivec2(params.width,params.height));
    vec2 force=vec2(0),v=vec2(b.vx_fp,b.vy_fp)/65536.0;float torque=0;
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 cell=ivec2(x,y);vec2 flow;float density;uint reaction_at;
        float fraction=hydro_sample(cell,flow,density,reaction_at);
        if(pooled) fraction=clamp(float(y)+1-waterline,0,1);
        if(fraction<=0) continue;
        vec2 query_low=vec2(cell)-origin;uvec4 query=body_piece_query(b,query_low,query_low+1);
        for(uint ordinal=0u;ordinal<query.z*query.w;ordinal++) {
            uint piece=query_piece(b,query,ordinal);
            vec2 point;float volume=body_cell_volume(b,piece,cell,fraction,point);
            if(volume<=0) continue;
            vec2 r=point-center,relative=flow-v-b.angular_velocity*vec2(-r.y,r.x);
            vec2 impulse=volume*(vec2(0,-.25*density)+WATER_DRAG_RATE*relative);
            force+=impulse;torque+=body_cross(r,impulse);
            hydro_phase_reaction(reaction_at,impulse);
            structural_capture(slot,BODY_CAPACITY,point,impulse);
        }
    }
    if((b.motion_flags&1u)==0u) {
        b.vx_fp+=int(round(force.x*b.inverse_mass*65536.0));
        b.vy_fp+=int(round(force.y*b.inverse_mass*65536.0));
        b.angular_velocity+=torque*b.inverse_inertia;
    }
}
