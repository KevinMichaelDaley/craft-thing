void body_surface_feedback(uint index) {
    vec4 metadata=samples[index+params.width*params.height];if(floatBitsToUint(metadata.x)!=4u) return;
    uvec2 handle=uvec2(floatBitsToUint(metadata.y),floatBitsToUint(metadata.w));
    if(handle.x>=root[2]||handle.y==0u) return;
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint s=FR_ENTRY_STATE,m=entry.words[s];
    if(entry.words[FR_ENTRY_GENERATION]!=handle.y||entry.words[FR_ENTRY_OWNER]!=2u||
        (m!=7u&&m!=8u&&m!=9u&&m!=12u)||entry.words[s+8u]==0u) return;
    float capacity=solid_capacity(entry);if(capacity<=0.) return;
    float coverage=clamp(samples[index].y/capacity,0.,1.);
    float char_ratio=float(entry.words[s+10u])/float(entry.words[s+8u]);
    float char_opacity=1.-exp(-char_ratio/float(FR_COMBUSTION_CHAR_OPTICAL_FRACTION));
    float temperature=(uintBitsToFloat(entry.words[s+17u])+uintBitsToFloat(entry.words[s+25u])+uintBitsToFloat(entry.words[s+26u]))/capacity;
    float power=0.;Words cache=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    if(cache.words[handle.x*12u+8u]==handle.y) {
        float surface=uintBitsToFloat(cache.words[handle.x*12u+9u]);if(surface>0.) temperature=surface;
        power=max(0.,uintBitsToFloat(cache.words[handle.x*12u+6u]));
    }
    /* Grain comes from the authored rest color already rendered here. Optical
     * charring can become visible before an entire centimeter voxel is char. */
    vec3 base=unpackUnorm4x8(pixels[index]).rgb;
    vec3 color=base*(1.-.93*char_opacity*coverage);
    float thermal=clamp((temperature-float(FR_COMBUSTION_SURFACE_GLOW_K))/850.,0.,1.);
    float reaction=1.-exp(-power*float(FR_COMBUSTION_SURFACE_EXPOSURE));
    float glow=thermal*max(reaction,.25*char_opacity)*coverage;
    vec3 ember=mix(vec3(1.,.035,.001),vec3(1.,.55,.08),clamp((temperature-800.)/1000.,0.,1.));
    color+=ember*glow;pixels[index]=packUnorm4x8(vec4(clamp(color,0.,1.),1.));
}
