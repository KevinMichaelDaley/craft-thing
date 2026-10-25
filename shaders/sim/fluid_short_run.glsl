/* In an open run with a common reach L>=2, an empty leading strip of
 * L-1 cells lets every source send all its mass to offsets L and L-1.
 * Far flux f[i]=min(allowance[i],mass[i],FULL-mass[i-1]+f[i-1]).
 * This is the same associative min-plus recurrence as one-cell transport.
 * Other flows retain the general ordered path. */
void short_run_publish() {
    subgroupMemoryBarrierShared();
    if(WATER_COLOR_ENABLED||thermal_water_on()) transport_buffer_barrier();
    subgroupBarrier();
}
void short_run_add(uint position,uint amount,vec3 pigment,float heat_density) {
    if(amount==0u) return;
    ivec2 point=point_at(position);uint receiving=water_at(point);
    if(WATER_COLOR_ENABLED) {
        uint index=atlas_index(point,page_at(point));
        water_colors[index]=vec4((water_color_at(index)*float(receiving)+pigment*float(amount))/
            (float(receiving)+float(amount)),1.);
    }
    put_water(point,receiving+amount);
    thermal_water_add(atlas_index(point,page_at(point)),heat_density*float(amount));
}
void short_run_front(int first,int direction,uint distance,uint count,
                     uint amount,vec3 pigment,float heat_density,bool far) {
    uint lane=gl_SubgroupInvocationID,team=transport_team_size();
    for(uint block=0u;block<distance;block+=team) {
        uint offset=block+lane+1u;
        int source=int(distance)-int(offset)-(far?0:1);
        uint index=uint(clamp(source,0,int(team)-1));
        uint incoming=subgroupShuffle(amount,index);
        float density=subgroupShuffle(heat_density,index);
        vec3 color=DEFAULT_WATER_COLOR;
        if(WATER_COLOR_ENABLED) color=subgroupShuffle(pigment,index);
        if(offset<=distance&&source>=0&&source<int(count))
            short_run_add(uint(first+direction*int(offset)),incoming,color,density);
    }
}
uint short_run_block(int first,bool positive) {
    uint lane=gl_SubgroupInvocationID,team=transport_team_size();
    int direction=positive?1:-1;
    uint face=uint(positive?first:first-1);
    float first_travel=uintBitsToFloat(cached[face].w);
    if(first_travel<=1.||first_travel>float(4u*team)) return 0u;
    uint distance=uint(ceil(first_travel));
    int front=first+direction*int(distance);
    if(front<0||front>=int(length)) return 0u;

    int coordinate=first-direction*int(lane);
    bool valid=coordinate>=0&&coordinate<int(length);
    uint position=valid?uint(coordinate):0u,mass=valid?cached[position].x:0u;
    uint meta=valid?cached[position].y:0u;
    bool simple=valid&&(meta&(KNOWN|PERMEABLE))==(KNOWN|PERMEABLE)&&
        (meta&131071u)==FULL&&mass<=FULL;
    float travel=0.;
    if(simple) {
        uint own_face=positive?position:position-1u;
        if(!positive&&position==0u) simple=false;
        else {
            travel=uintBitsToFloat(cached[own_face].w);
            simple=(cached[own_face].z&131071u)==FULL&&(mass==0u||ceil(travel)==float(distance));
        }
    }
    uint count=subgroupMin(simple?team:lane);
    if(count<4u) return 0u;
    if(subgroupAdd(uint(lane<count&&mass!=0u))<4u) return 0u;
    bool clear_front=true;
    for(uint block=0u;block<distance;block+=team) {
        uint offset=block+lane+1u;
        if(offset>distance) continue;
        uint p=uint(first+direction*int(offset)),state=cached[p].y;
        clear_front=clear_front&&(state&(KNOWN|PERMEABLE))==(KNOWN|PERMEABLE)&&
            (state&131071u)==FULL&&cached[p].x<=FULL;
        if(offset<distance) {
            uint edge=positive?p:p-1u;
            clear_front=clear_front&&cached[p].x==0u&&(cached[edge].z&131071u)==FULL;
        }
    }
    if(subgroupMin(uint(clear_front))==0u) return 0u;

    uint allowance=uint(floor(clamp(travel-float(distance-1u),0.,1.)*float(FULL)));
    uint previous=subgroupShuffleUp(mass,1u);
    uvec2 value=uvec2(min(allowance,mass),lane==0u?FULL-cached[uint(front)].x:FULL-previous);
    for(uint span=1u;span<team;span*=2u) {
        uvec2 prior=subgroupShuffleUp(value,span);
        if(lane>=span) value=uvec2(min(value.x,value.y+prior.x),value.y+prior.y);
    }
    uint far_flux=lane<count?min(value.x,value.y):0u;
    uint near_flux=lane<count?mass-far_flux:0u;
    vec3 pigment=DEFAULT_WATER_COLOR;
    float heat_density=0.;
    if(thermal_water_on()&&lane<count&&mass!=0u) heat_density=thermal_water_at(atlas_index(point_at(position),page_at(point_at(position))))/float(mass);
    if(WATER_COLOR_ENABLED&&lane<count) pigment=water_color_at(atlas_index(point_at(position),page_at(point_at(position))));
    short_run_publish();
    if(lane<count&&mass!=0u) put_water(point_at(position),0u);
    short_run_publish();

    /* Each recipient gets the nearer source first, then the farther source.
       Publish between phases to retain the original float pigment rounding. */
    uint near_in=subgroupShuffleDown(near_flux,min(distance-1u,team-1u));
    vec3 near_color=DEFAULT_WATER_COLOR;
    if(WATER_COLOR_ENABLED) near_color=subgroupShuffleDown(pigment,min(distance-1u,team-1u));
    float near_heat=subgroupShuffleDown(heat_density,min(distance-1u,team-1u));
    if(lane<count&&lane+distance-1u<count) short_run_add(position,near_in,near_color,near_heat);
    short_run_front(first,direction,distance,count,near_flux,pigment,heat_density,false);
    short_run_publish();
    uint far_in=subgroupShuffleDown(far_flux,min(distance,team-1u));
    vec3 far_color=DEFAULT_WATER_COLOR;
    if(WATER_COLOR_ENABLED) far_color=subgroupShuffleDown(pigment,min(distance,team-1u));
    float far_heat=subgroupShuffleDown(heat_density,min(distance,team-1u));
    if(lane<count&&lane+distance<count) short_run_add(position,far_in,far_color,far_heat);
    short_run_front(first,direction,distance,count,far_flux,pigment,heat_density,true);
    short_run_publish();
    return count;
}
