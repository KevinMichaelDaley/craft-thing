#ifndef DC_CONTACT_BOUNDARY_CACHE_GLSL
#define DC_CONTACT_BOUNDARY_CACHE_GLSL
#ifdef DC_RUNTIME_BODY_STORAGE
uint boundary_cache_base() {
    uvec2 extent=page_extent();
    return 4u+DC_LAYOUT_PAIR_CAPACITY*DC_CONTACT_PIXEL_GROUPS*2u+extent.x*extent.y;
}
void cache_body_boundaries() {
    uint lane=gl_LocalInvocationIndex;
    uint slot=gl_WorkGroupID.x+gl_WorkGroupID.z*gl_NumWorkGroups.x;
    uint pool_low=params.origin_x.x;
    if(slot==0u&&gl_WorkGroupID.y==0u&&lane==0u) DC_RUNTIME_WORK(52u)=pool_low;
    if(slot>=params.body_count) return;
    Body body=DC_BODY(slot);
    if(!pixel_body(body)||(body.enabled==0u&&body.reserved0==0u)) return;
    uint capacity=2u*DC_RUNTIME_WORK(29u),base=boundary_cache_base();
    uint stride=gl_NumWorkGroups.y*gl_WorkGroupSize.x;
    for(uint word=gl_WorkGroupID.y*gl_WorkGroupSize.x+lane;
        word<pixel_tile_count(body)*32u;word+=stride) {
        uint address=body.compound_reserved+pixel_tile_offset(body,word/32u)+1u+word%32u;
        uint relative=address-pool_low;
        /* Aliased immutable shapes have the same mask; atomics also make
           sharing safe when a checkpoint retains duplicate shape references. */
        if(relative<capacity) atomicExchange(DC_CONTACT_WORK(base+relative),pixel_boundary_mask(body,word));
    }
}
#endif
#endif
