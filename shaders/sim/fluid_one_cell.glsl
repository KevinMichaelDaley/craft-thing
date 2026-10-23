/* A maximal simple prefix of the ordered source stream is evaluated by a
 * subgroup min-plus scan. Complex sources retain the general transport path.
 * For outgoing flux f[i], f[i]=min(allowance[i], free[i]+f[i-1]). */
uint one_cell_block(int first,bool positive) {
    uint lane=gl_SubgroupInvocationID,team=transport_team_size();
    int coordinate=first+(positive?-int(lane):int(lane));
    bool valid=coordinate>=0&&coordinate<int(length);
    uint position=valid?uint(coordinate):0u,target=positive?position+1u:position-1u;
    bool target_valid=valid&&(positive?position+1u<length:position>0u);
    uint mass=valid?cached[position].x:0u;
    uint meta=valid?cached[position].y:0u,next_meta=target_valid?cached[target].y:0u;
    uint next_mass=target_valid?cached[target].x:0u,allowance=0u;
    bool source=valid&&(source_bits[position/32u]&(1u<<(position%32u)))!=0u;
    bool simple=target_valid;
    if(source) {
        uint face=positive?position:target;
        float travel=uintBitsToFloat(cached[face].w);
        simple=simple&&mass<=(meta&131071u)&&next_mass<=(next_meta&131071u)&&travel<=1.;
        if((next_meta&(KNOWN|PERMEABLE))==(KNOWN|PERMEABLE)&&(next_meta&131071u)!=0u) {
            float aperture=line_open_aperture?1.:float((cached[face].z&131071u))/float(FULL);
            allowance=uint(floor(clamp(travel,0.,1.)*float(FULL)*aperture));
        }
    }
    uint count=subgroupMin(simple?team:lane);
    if(count==0u) return 0u;
    uvec2 value=uvec2(min(allowance,mass),next_mass<=(next_meta&131071u)?(next_meta&131071u)-next_mass:0u);
    for(uint span=1u;span<team;span*=2u) {
        uvec2 prior=subgroupShuffleUp(value,span);
        if(lane>=span) value=uvec2(min(value.x,value.y+prior.x),value.y+prior.y);
    }
    uint outgoing=lane<count?min(value.x,value.y):0u;
    if(subgroupOr(outgoing)==0u) return count;
    uint incoming=subgroupShuffleDown(outgoing,1u);
    if(lane+1u>=count) incoming=0u;
    vec3 color=DEFAULT_WATER_COLOR,lower_color=DEFAULT_WATER_COLOR,terminal_color=DEFAULT_WATER_COLOR;
    if(WATER_COLOR_ENABLED) {
        if(valid&&page_at(point_at(position))!=0u) color=water_color_at(atlas_index(point_at(position),page_at(point_at(position))));
        lower_color=subgroupShuffleDown(color,1u);
        if(lane==0u&&page_at(point_at(target))!=0u) terminal_color=water_color_at(atlas_index(point_at(target),page_at(point_at(target))));
        transport_color_publish();
    }
    if(lane<count&&(outgoing!=0u||incoming!=0u)) {
        uint remaining=mass-outgoing;
        put_water(point_at(position),remaining+incoming);
        if(WATER_COLOR_ENABLED&&incoming!=0u) water_colors[atlas_index(point_at(position),page_at(point_at(position)))]=
            vec4((color*float(remaining)+lower_color*float(incoming))/(float(remaining)+float(incoming)),1.);
    }
    if(lane==0u&&outgoing!=0u) {
        put_water(point_at(target),next_mass+outgoing);
        if(WATER_COLOR_ENABLED) water_colors[atlas_index(point_at(target),page_at(point_at(target)))]=
            vec4((terminal_color*float(next_mass)+color*float(outgoing))/(float(next_mass)+float(outgoing)),1.);
    }
    subgroupMemoryBarrierShared();transport_color_publish();subgroupBarrier();
    return count;
}
