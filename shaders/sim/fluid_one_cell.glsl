/* Ordered one-cell flux satisfies f[i]=min(a[i],b[i]+f[i+1]), where a is
 * the source allowance and b is the recipient's initial free capacity.
 * Composition of (a,b) is associative: (min(a,b+c),b+d). A suffix scan
 * therefore preserves the serial integer recurrence without serial sources.
 * Nonnegative b and travel <=1 are checked before entering this path. */
uvec2 one_cell_state(uint i,bool alternate) {
    return alternate?cached[i].wy:uvec2(cached_requests[i],cached[i].z);
}
void one_cell_scan(bool positive) {
    uint lane=gl_SubgroupInvocationID,team=transport_team_size(),any_request=0u;
    for(uint oriented=lane;oriented<length;oriented+=team) {
        uint position=positive?oriented:length-1u-oriented,request=0u;
        if(oriented+1u<length) {
            uint target=positive?position+1u:position-1u,face=positive?position:target;
            uint source_meta=cached[position].y,target_meta=cached[target].y;
            if((source_meta&(KNOWN|PERMEABLE))==(KNOWN|PERMEABLE)&&
               (target_meta&(KNOWN|PERMEABLE))==(KNOWN|PERMEABLE)&&
               (source_meta&131071u)!=0u&&(target_meta&131071u)!=0u) {
                float travel=uintBitsToFloat(cached[face].w);
                float aperture=line_open_aperture?1.:float(cached[face].z)/float(FULL);
                uint allowance=uint(floor(clamp(travel,0.,1.)*float(FULL)*aperture));
                request=min(allowance,cached[position].x);
            }
        }
        cached_requests[oriented]=request;any_request|=request;
    }
    subgroupMemoryBarrierShared();subgroupBarrier();
    if(subgroupOr(any_request)==0u) return;
    for(uint oriented=lane;oriented<length;oriented+=team) {
        uint free_capacity=0u;
        if(oriented+1u<length) {
            uint target=positive?oriented+1u:length-2u-oriented;
            free_capacity=(cached[target].y&131071u)-cached[target].x;
        }
        cached[oriented].z=free_capacity;
    }
    subgroupMemoryBarrierShared();subgroupBarrier();
    bool alternate=false;
    for(uint span=1u;span<length;span*=2u) {
        for(uint i=lane;i<length;i+=team) {
            uvec2 value=one_cell_state(i,alternate);
            if(i+span<length) {
                uvec2 next=one_cell_state(i+span,alternate);
                value=uvec2(min(value.x,value.y+next.x),value.y+next.y);
            }
            if(alternate) {cached_requests[i]=value.x;cached[i].z=value.y;}
            else {cached[i].w=value.x;cached[i].y=value.y;}
        }
        subgroupMemoryBarrierShared();subgroupBarrier();alternate=!alternate;
    }
    for(uint i=lane;i<length;i+=team) cached_requests[i]=one_cell_state(i,alternate).x;
    subgroupMemoryBarrierShared();subgroupBarrier();
    if(WATER_COLOR_ENABLED) {
        for(uint position=lane;position<length;position+=team) {
            ivec2 p=point_at(position);uint page=page_at(p);
            vec3 color=page==0u?DEFAULT_WATER_COLOR:water_color_at(atlas_index(p,page));
            cached[position].yzw=floatBitsToUint(color);
        }
        subgroupMemoryBarrierShared();subgroupBarrier();
    }
    for(uint oriented=lane;oriented<length;oriented+=team) {
        uint position=positive?oriented:length-1u-oriented;
        ivec2 p=point_at(position);uint page=page_at(p);if(page==0u) continue;
        uint outgoing=cached_requests[oriented],incoming=oriented==0u?0u:cached_requests[oriented-1u];
        uint receiving=cached[position].x-outgoing,a=atlas_index(p,page);
        chunk_cells[a].y=receiving+incoming;
        if(WATER_COLOR_ENABLED) {
            if(incoming!=0u) {
                uint source=positive?position-1u:position+1u;
                vec3 color=(uintBitsToFloat(cached[position].yzw)*float(receiving)+
                    uintBitsToFloat(cached[source].yzw)*float(incoming))/(float(receiving)+float(incoming));
                water_colors[a]=vec4(color,1.);
            } else if(outgoing!=0u&&receiving==0u) water_colors[a]=vec4(0);
        }
    }
}
