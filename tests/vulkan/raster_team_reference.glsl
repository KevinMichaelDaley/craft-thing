/* Frozen ascending-slot Hillis-Steele scan used by the pre-bitset raster. */
void select_raster_team(uint lane,uint slot,bool selected) {
    if(lane<64u) raster_legacy[lane]=uint(selected);
    raster_barrier();
    for(uint stride=1u;stride<64u;stride*=2u) {
        uint prior=lane<64u&&lane>=stride?raster_legacy[lane-stride]:0u;
        raster_barrier();if(lane<64u) raster_legacy[lane]+=prior;raster_barrier();
    }
    if(lane<64u&&selected) raster_ids[raster_legacy[lane]-1u]=slot;
    if(lane==0u) raster_count=raster_legacy[63];
    raster_barrier();
}
