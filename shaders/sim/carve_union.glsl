/* Monotone links cannot cycle. Atomic path halving makes every edge merge
 * independent of the scheduling order, without a diameter-dependent frontier. */
shared uint carve_root_counts[DC_STRUCTURAL_LANES];
uint carve_union_root(uint pixel) {
    for(;;) {
        uint parent=atomicAdd(DC_FRAGMENT_LABEL(pixel),0u)-1u;
        if(parent==pixel) return pixel;
        uint grandparent=atomicAdd(DC_FRAGMENT_LABEL(parent),0u)-1u;
        atomicMin(DC_FRAGMENT_LABEL(pixel),grandparent+1u);
        pixel=grandparent;
    }
}
void carve_union_join(uint a,uint b) {
    for(;;) {
        uint ra=carve_union_root(a),rb=carve_union_root(b);
        if(ra==rb) return;
        uint high=max(ra,rb),low=min(ra,rb);
        uint previous=atomicMin(DC_FRAGMENT_LABEL(high),low+1u);
        if(previous==high+1u) return;
    }
}
bool carve_union_components(Body source,uint area) {
    uint lane=gl_LocalInvocationIndex;
    for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES)
        if(DC_FRAGMENT_LABEL(pixel)==0u) DC_FRAGMENT_LABEL(pixel)=pixel+1u;
    memoryBarrierBuffer();barrier();
    const ivec2 edges[4]=ivec2[4](ivec2(1,0),ivec2(-1,1),ivec2(0,1),ivec2(1,1));
    for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES) {
        if(atomicAdd(DC_FRAGMENT_LABEL(pixel),0u)==0xffffffffu) continue;
        ivec2 p=pixel_piece_cell(source,pixel);
        for(uint edge=0u;edge<4u;edge++) {
            ivec2 q=p+edges[edge];
            if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(source.width,source.height)))) continue;
            uint next=carve_neighbor_piece(source,pixel,edges[edge],q);
            if(next!=0xffffffffu&&atomicAdd(DC_FRAGMENT_LABEL(next),0u)!=0xffffffffu)
                carve_union_join(pixel,next);
        }
    }
    memoryBarrierBuffer();barrier();
    for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES)
        if(atomicAdd(DC_FRAGMENT_LABEL(pixel),0u)!=0xffffffffu) {
            uint root=carve_union_root(pixel);
            atomicMin(DC_FRAGMENT_LABEL(pixel),root+1u);
        }
    memoryBarrierBuffer();barrier();
    /* Contiguous lane ranges and a shared prefix rank roots by their original
     * pixel index, preserving the flood fill's source-seed fragment order. */
    uint chunk=area/DC_STRUCTURAL_LANES,extra=area%DC_STRUCTURAL_LANES;
    uint begin=lane*chunk+min(lane,extra),end=begin+chunk+uint(lane<extra),count=0u;
    for(uint pixel=begin;pixel<end;pixel++) count+=uint(DC_FRAGMENT_LABEL(pixel)==pixel+1u);
    carve_root_counts[lane]=count;barrier();
    for(uint stride=1u;stride<DC_STRUCTURAL_LANES;stride*=2u) {
        uint sum=carve_root_counts[lane]+(lane>=stride?carve_root_counts[lane-stride]:0u);
        barrier();carve_root_counts[lane]=sum;barrier();
    }
    if(lane==0u) {
        pc_count=carve_root_counts[DC_STRUCTURAL_LANES-1u];
        if(pc_count>pc_available) {
            atomicMax(DC_RUNTIME_REQUEST(2u),BODY_CAPACITY+pc_count-pc_available);
            atomicOr(DC_RUNTIME_REQUEST(10u),1u);
        }
    }
    memoryBarrierBuffer();barrier();
    if(pc_count>pc_available) return false;
    uint ordinal=lane==0u?0u:carve_root_counts[lane-1u];
    for(uint pixel=begin;pixel<end;pixel++) if(DC_FRAGMENT_LABEL(pixel)==pixel+1u) {
        DC_FRAGMENT_QUEUE(pixel)=ordinal;
        PF_MATERIAL(ordinal)=piece_material(source,pixel);ordinal++;
    }
    memoryBarrierBuffer();barrier();
    for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES) {
        uint root=DC_FRAGMENT_LABEL(pixel);
        if(root!=0xffffffffu) DC_FRAGMENT_LABEL(pixel)=DC_FRAGMENT_QUEUE(root-1u)+1u;
    }
    memoryBarrierBuffer();barrier();
    for(uint owner=1u;owner<=pc_count;owner++) {
        ivec2 low=ivec2(0x7fffffff),high=ivec2(0);uint size=0u;
        for(uint pixel=lane;pixel<area;pixel+=DC_STRUCTURAL_LANES) if(DC_FRAGMENT_LABEL(pixel)==owner) {
            ivec2 p=pixel_piece_cell(source,pixel);low=min(low,p);high=max(high,p+1);size++;
        }
        pc_lows[lane]=low;pc_highs[lane]=high;carve_root_counts[lane]=size;barrier();
        if(lane==0u) {
            for(uint other=1u;other<DC_STRUCTURAL_LANES;other++) {
                low=min(low,pc_lows[other]);high=max(high,pc_highs[other]);size+=carve_root_counts[other];
            }
            PF_LOW(owner-1u)=low;PF_HIGH(owner-1u)=high;PF_COUNT(owner-1u)=size;
        }
        memoryBarrierBuffer();barrier();
    }
    return true;
}
