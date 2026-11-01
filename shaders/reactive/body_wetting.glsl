float body_water_contact(Body body,uint piece,ivec2 cell,float fraction) {
    vec2 center;float overlap=body_cell_volume(body,piece,cell,fraction,center);
    vec2 low=vec2(cell)+vec2(0,1.-fraction),high=vec2(cell)+1.;
    float edges=body_face_coverage(body,piece,low,vec2(high.x,low.y))+
        body_face_coverage(body,piece,vec2(high.x,low.y),high)*fraction+
        body_face_coverage(body,piece,high,vec2(low.x,high.y))+
        body_face_coverage(body,piece,vec2(low.x,high.y),low)*fraction;
    return min(4.,max(edges,4.*sqrt(max(overlap,0.))));
}
void body_water_energy_add(uint at,float energy) {
    uint old=atomicAdd(water_heat_words[at],0u);
    for(;;) {uint next=floatBitsToUint(uintBitsToFloat(old)+energy);
        uint observed=atomicCompSwap(water_heat_words[at],old,next);if(observed==old) break;old=observed;}
}
uint body_water_reserve(uint at,uint requested) {
    uint old=atomicAdd(cells[at].y,0u);
    while(old>0u&&requested>0u) {
        uint taken=min(old,requested),observed=atomicCompSwap(cells[at].y,old,old-taken);
        if(observed==old) {if(old==taken&&(cells[at].x&65535u)==3u) atomicAnd(cells[at].x,0xffff0000u);return taken;}
        old=observed;
    }
    return 0u;
}
void body_wet_parcel(Body body,uint piece,uvec2 handle) {
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint s=FR_ENTRY_STATE,m=entry.words[s];
    float capacity=round(float(entry.words[s+8u])*coefficient(m,19u));
    if(capacity<=float(entry.words[s+12u])) return;
    uint room=uint(capacity-float(entry.words[s+12u]));BodyQuad quad;if(!body_pixel_quad(body,piece,quad)) return;
    vec2 origin=vec2(body.x_fp,body.y_fp)/65536.;
    ivec2 low=ivec2(floor(origin+min(min(quad.a,quad.b),min(quad.c,quad.d))-.0001));
    ivec2 high=ivec2(floor(origin+max(max(quad.a,quad.b),max(quad.c,quad.d))+.0001));
    ParcelSamples cache=ParcelSamples(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    float remainder=floatBitsToUint(cache.values[handle.x*3u].w)==handle.y?cache.values[handle.x*3u+1u].w:0.;
    uint transferred=0u;float heat=0.;
    for(int y=low.y;y<=high.y;y++) for(int x=low.x;x<=high.x;x++) {
        ivec2 cell=ivec2(x,y);if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) continue;
        uint index=uint(y)*params.width+uint(x),at=atlas_at(index);if(at==0xffffffffu) continue;
        vec4 source=fluxes[index+params.width*params.height];uint available=floatBitsToUint(source.y);if(available==0u) continue;
        float contact=body_water_contact(body,piece,cell,min(1.,float(available)/65536.));if(contact<=0.) continue;
        float exposure=remainder+uintBitsToFloat(params.seconds)*float(FR_THERMAL_WETTING_PER_SECOND)*65536.*contact/4.;
        uint requested=min(room,uint(floor(exposure)));remainder=fract(exposure);
        uint taken=body_water_reserve(at,requested);if(taken==0u) continue;
        float excess=source.x*(float(taken)/float(available));body_water_energy_add(at,-excess);
        heat+=excess+float(taken)/65536.*coefficient(3u,5u)*float(FR_THERMAL_AMBIENT_K);
        room-=taken;transferred+=taken;
    }
    cache.values[handle.x*3u].w=uintBitsToFloat(handle.y);cache.values[handle.x*3u+1u].w=remainder;
    if(transferred>0u) {entry.words[s+12u]+=transferred;add_energy(entry,heat);}
}

uint body_oil_reserve(uint at,uint requested) {
    uint old=atomicAdd(bulk_phase_words[at],0u);
    while(old>0u&&requested>0u) {
        uint taken=min(old,requested);
        uint observed=atomicCompSwap(bulk_phase_words[at],old,old-taken);
        if(observed==old) return taken;
        old=observed;
    }
    return 0u;
}
void body_oil_heat_add(uint at,float energy) {
    uint old=atomicAdd(bulk_phase_words[at],0u);
    for(;;) {
        uint next=floatBitsToUint(uintBitsToFloat(old)+energy);
        uint observed=atomicCompSwap(bulk_phase_words[at],old,next);
        if(observed==old) return;
        old=observed;
    }
}
void body_oil_coat_parcel(Body body,uint piece,uvec2 handle) {
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));
    uint s=FR_ENTRY_STATE;
    float density=coefficient(14u,4u),capacity=coefficient(14u,5u);
    uint held=entry.words[s+FR_STATE_COATING_OIL];
    if(held>=65536u||density<=0.||capacity<=0.) return;
    BodyQuad quad;if(!body_pixel_quad(body,piece,quad)) return;
    vec2 origin=vec2(body.x_fp,body.y_fp)/65536.;
    ivec2 low=ivec2(floor(origin+min(min(quad.a,quad.b),min(quad.c,quad.d))-.0001));
    ivec2 high=ivec2(floor(origin+max(max(quad.a,quad.b),max(quad.c,quad.d))+.0001));
    uint gained=0u;float heat=0.;
    for(int y=low.y;y<=high.y;y++) for(int x=low.x;x<=high.x;x++) {
        ivec2 cell=ivec2(x,y);
        if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) continue;
        uint index=uint(y)*params.width+uint(x),at=atlas_at(index);
        if(at==0xffffffffu) continue;
        uint base=(at/4096u)*FR_BULK_CHUNK_CHANNELS*4096u+at%4096u;
        uint available=atomicAdd(bulk_phase_words[base],0u);
        if(available==0u) continue;
        float contact=body_water_contact(body,piece,cell,min(1.,float(available)/65536.));
        if(contact<=0.) continue;
        uint room=65536u-held-gained;
        uint request=uint(floor(float(FR_THERMAL_WETTING_PER_SECOND)*
            uintBitsToFloat(params.seconds)*65536.*contact/4.));
        request=min(request,uint(floor(float(room)/density)));
        uint taken=body_oil_reserve(base,request);
        if(taken==0u) continue;
        uint mass=uint(round(float(taken)*density));
        uint heat_at=base+4u*4096u;
        float excess=uintBitsToFloat(atomicAdd(bulk_phase_words[heat_at],0u))*
            float(taken)/float(available);
        body_oil_heat_add(heat_at,-excess);
        heat+=excess+float(mass)/65536.*capacity*float(FR_THERMAL_AMBIENT_K);
        gained+=mass;
    }
    if(gained>0u) {entry.words[s+FR_STATE_COATING_OIL]+=gained;add_energy(entry,heat);}
}
