#ifndef DC_CARVE_UNION_LINKS_GLSL
#define DC_CARVE_UNION_LINKS_GLSL

uint carve_union_root(uint pixel) {
    for(;;) {
        uint parent=atomicAdd(DC_FRAGMENT_LABEL(pixel),0u)-1u;
        if(parent==pixel) return pixel;
        uint grandparent=atomicAdd(DC_FRAGMENT_LABEL(parent),0u)-1u;
        atomicMin(DC_FRAGMENT_LABEL(pixel),grandparent+1u);
        pixel=grandparent;
    }
}

uint carve_union_attach_root(uint high,uint low) {
    /* A stale root may already lead to a different component that must stay linked. */
    return atomicCompSwap(DC_FRAGMENT_LABEL(high),high+1u,low+1u);
}

void carve_union_join(uint a,uint b) {
    for(;;) {
        uint ra=carve_union_root(a),rb=carve_union_root(b);
        if(ra==rb) return;
        uint high=max(ra,rb),low=min(ra,rb);
        if(carve_union_attach_root(high,low)==high+1u) return;
    }
}

#endif
