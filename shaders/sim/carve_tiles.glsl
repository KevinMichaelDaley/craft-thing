shared uint carve_tile_prefix[DC_STRUCTURAL_LANES];
bool carve_tile_contains(uint tile,uint owner) {
    for(uint cell=0u;cell<256u;cell++)
        if(DC_FRAGMENT_LABEL(tile*256u+cell)==owner) return true;
    return false;
}
void carve_count_tiles(Body source) {
    uint lane=gl_LocalInvocationIndex,tiles=pixel_tile_count(source);
    for(uint child=0u;child<pc_count;child++) {
        uint count=0u;
        for(uint tile=lane;tile<tiles;tile+=DC_STRUCTURAL_LANES)
            count+=uint(carve_tile_contains(tile,child+1u));
        carve_tile_prefix[lane]=count;barrier();
        if(lane==0u) {
            for(uint other=1u;other<DC_STRUCTURAL_LANES;other++) count+=carve_tile_prefix[other];
            PF_TILES(child)=count;
        }
        memoryBarrierBuffer();barrier();
    }
}
void carve_pack_tiles(Body source,uint child,uint pointer,uint directory) {
    uint lane=gl_LocalInvocationIndex,tiles=pixel_tile_count(source);
    uint chunk=tiles/DC_STRUCTURAL_LANES,extra=tiles%DC_STRUCTURAL_LANES;
    uint begin=lane*chunk+min(lane,extra),end=begin+chunk+uint(lane<extra),count=0u;
    for(uint tile=begin;tile<end;tile++) {
        uint present=uint(carve_tile_contains(tile,child+1u));
        DC_FRAGMENT_QUEUE(tile)=present;count+=present;
    }
    carve_tile_prefix[lane]=count;barrier();
    for(uint stride=1u;stride<DC_STRUCTURAL_LANES;stride*=2u) {
        uint sum=carve_tile_prefix[lane]+(lane>=stride?carve_tile_prefix[lane-stride]:0u);
        barrier();carve_tile_prefix[lane]=sum;barrier();
    }
    uint ordinal=lane==0u?0u:carve_tile_prefix[lane-1u];
    for(uint tile=begin;tile<end;tile++)
        if(DC_FRAGMENT_QUEUE(tile)!=0u) DC_FRAGMENT_QUEUE(tile)=++ordinal;
    memoryBarrierBuffer();barrier();
#ifdef DC_CARVE_WORK_PROFILE
    if(lane==0u) atomicAdd(carve_work_profile[4],16u);
#endif
    uint width=pixel_word_at(source.compound_reserved+2u);
    for(uint tile=lane;tile<tiles;tile+=DC_STRUCTURAL_LANES) {
        uint rank=DC_FRAGMENT_QUEUE(tile);if(rank==0u) continue;
        uint offset=8u+directory+(rank-1u)*33u;
        uint coord=pixel_word_at(source.compound_reserved+8u+directory+tile*33u);
        DC_DATA(pointer+offset)=coord;
        DC_DATA(pointer+8u+(coord>>16u)*width+(coord&65535u))=offset;
    }
    for(uint word=lane;word<tiles*32u;word+=DC_STRUCTURAL_LANES) {
        uint tile=word/32u,part=word%32u,rank=DC_FRAGMENT_QUEUE(tile);
        if(rank==0u) continue;
        uint source_word=source.compound_reserved+8u+directory+tile*33u+1u+part;
        uint mask=0u;
        for(uint nibble=0u;nibble<8u;nibble++)
            if(DC_FRAGMENT_LABEL(tile*256u+part*8u+nibble)==child+1u) mask|=15u<<(nibble*4u);
        uint target_word=pointer+8u+directory+(rank-1u)*33u+1u+part;
        DC_DATA(target_word)=pixel_word_at(source_word)&mask;
        shape_color_stage(BODY_STORAGE_WORDS+target_word,source_word);
    }
}
