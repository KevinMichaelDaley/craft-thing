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
