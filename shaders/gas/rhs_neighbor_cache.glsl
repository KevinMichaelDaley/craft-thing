shared vec4 rhs_neighbor_forecast[100];
shared vec2 rhs_neighbor_head[100];
uint gas_rhs_neighbor_slot(uint direction) {
    uint lane=gl_LocalInvocationIndex,center=(lane/8u+1u)*10u+lane%8u+1u;
    return direction==0u?center+1u:(direction==1u?center-1u:(direction==2u?center+10u:center-10u));
}
void gas_rhs_cache_prepare() {
    uint tile=active_tiles[2u*gas_tile_count()+gl_WorkGroupID.x],width=(params.width+7u)/8u;
    ivec2 origin=ivec2(int(tile%width),int(tile/width))*8-1;
    for(uint slot=gl_LocalInvocationIndex;slot<100u;slot+=64u) {
        ivec2 p=origin+ivec2(int(slot%10u),int(slot/10u));
        vec4 forecast=vec4(0);vec2 head=vec2(0);
        if(all(greaterThanEqual(p,ivec2(0)))&&all(lessThan(p,ivec2(params.width,params.height)))) {
            uint index=uint(p.y)*params.width+uint(p.x);Primitive other=gas_cached(p);
            if(other.metric.w>0.) {
                if(gas_advected(index)) {
                    Gas transported=gas_practical_state(gas_provisional(gas_atlas(p)));
                    forecast=gas_rhs_neighbor(other,transported);head=gas_large_pressure_state(transported,other.hydro.w);
                } else {
                    forecast=vec4(other.hydro.yz*other.metric.z,0.,other.metric.w);
                    Primitive stage=gas_rhs_stage(p,index);head=vec2(stage.hydro.x,stage.metric.x);
                }
            }
        }
        rhs_neighbor_forecast[slot]=forecast;rhs_neighbor_head[slot]=head;
    }
    if(gl_WorkGroupID.x==0u&&gl_LocalInvocationIndex==0u) atomicOr(stats[15],65536u);
    barrier();
}
