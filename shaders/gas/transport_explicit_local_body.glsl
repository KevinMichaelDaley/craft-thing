GasFlux local_stage_face(ivec2 p,float seconds,float positive,float negative) {
    ivec2 step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
    return transport_add(transport_scale(line_source(p),vec2(seconds*positive)),transport_scale(line_source(p+step),vec2(-seconds*negative)));
}
void FR_GAS_EXPLICIT_ENTRY() {
    uint rank=gl_LocalInvocationIndex,length=transport_axis()==0u?params.width:params.height,line=gl_WorkGroupID.x;
    if(rank==0u) {transport_work[line*length].mass.w=0.;if(line==0u) atomicOr(stats[15],262144u);}
    if(length>FR_GAS_EXPLICIT_LINE_CAPACITY||(FR_GAS_EXPLICIT_LINE_CAPACITY+FR_GAS_LOCAL_SIZE-1u)/FR_GAS_LOCAL_SIZE!=3u) {if(rank==0u) atomicAdd(stats[FR_GAS_IMPLICIT_LINE_COUNT_WORD],1u);return;}
    float speed=0.;
    for(uint cell=rank;cell<length;cell+=FR_GAS_LOCAL_SIZE) speed=max(speed,transport_activity_speed(transport_point(line,cell)));
    speed=line_maximum(speed);
    if(speed==0.) {if(rank==0u) {transport_work[line*length].mass.w=-1.;atomicAdd(stats[FR_GAS_INACTIVE_LINE_COUNT_WORD],1u);}return;}
    float seconds=uintBitsToFloat(params.seconds),courant=seconds*max(speed,FR_GAS_PRACTICAL_MIXING_RATE);
    if(!(courant<=.9*float(FR_GAS_EXPLICIT_MAX_STAGES))) {if(rank==0u) atomicAdd(stats[FR_GAS_IMPLICIT_LINE_COUNT_WORD],1u);return;}
    uint stages=max(uint(ceil(courant/.9)),1u);float peak=0.;
    const uint count=(FR_GAS_EXPLICIT_LINE_CAPACITY+FR_GAS_LOCAL_SIZE-1u)/FR_GAS_LOCAL_SIZE;
    vec4 rates[count];uint atlases[count];
    [[unroll]] for(uint k=0u;k<count;k++) {
        uint cell=rank+k*FR_GAS_LOCAL_SIZE;if(cell>=length) continue;
        ivec2 p=transport_point(line,cell),step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
        atlases[k]=gas_atlas(p);Gas initial=atlases[k]==0xffffffffu?gas_empty():transport_axis_input(atlases[k]);
        rates[k]=vec4(transport_rates(p,-1).x,transport_rates(p,1).x,transport_rates(p-step,1).x,transport_rates(p+step,-1).x);
        GasFlux value=transport_state(initial);line_species[cell]=value.species;line_dynamics[cell]=value.dynamics;line_visual[cell]=vec4(value.pigment,0);
        peak=max(peak,max(value.species.z,value.dynamics.x));
    }
    peak=line_maximum(peak);
    float scale=peak<=3.402823466e38/(float(length+1u)*4294967296.*16.)?4294967296.:1.;
    if(rank==0u) {transport_work[line*length].mass.w=-2.;atomicAdd(stats[FR_GAS_EXPLICIT_LINE_COUNT_WORD],1u);atomicAdd(stats[FR_GAS_EXPLICIT_STAGE_COUNT_WORD],stages);}
    float interval=seconds/float(stages);
    for(uint stage=0u;stage<stages;stage++) {
        [[unroll]] for(uint k=0u;k<count;k++) {
            uint cell=rank+k*FR_GAS_LOCAL_SIZE;if(cell>=length) continue;
            if(stage!=0u) {GasFlux value=atlases[k]==0xffffffffu?gas_empty_flux():transport_state(gas_provisional(atlases[k]));line_species[cell]=value.species;line_dynamics[cell]=value.dynamics;line_visual[cell]=vec4(value.pigment,0);}
            line_species[cell].z*=scale;line_dynamics[cell].x*=scale;line_visual[cell].rgb*=scale;
        }
        barrier();
        [[dont_unroll]] for(uint k=0u;k<count;k++) {
            uint cell=rank+k*FR_GAS_LOCAL_SIZE;if(cell>=length) continue;
            uint atlas=k==0u?atlases[0]:(k==1u?atlases[1]:atlases[2]);if(atlas==0xffffffffu) continue;
            vec4 own_rates=k==0u?rates[0]:(k==1u?rates[1]:rates[2]);
            ivec2 p=transport_point(line,cell),step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
            GasFlux incoming=local_stage_face(p-step,interval,own_rates.z,own_rates.x),outgoing=local_stage_face(p,interval,own_rates.y,own_rates.w);
            Gas previous=stage==0u?transport_axis_input(atlas):gas_provisional(atlas);
            gas_write_provisional(atlas,transport_update_state(previous,incoming,outgoing,scale));
        }
        /* Order candidate-buffer reloads before the next shared transport stage. */
        memoryBarrierBuffer();barrier();
    }
}
