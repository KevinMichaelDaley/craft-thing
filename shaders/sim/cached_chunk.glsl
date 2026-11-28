// Structural registry records contain signed world coordinates and an active flag.
uint cached_chunk_page(uint registry,uint count,uvec2 x,uvec2 y) {
    for(uint slot=0u;slot<count;++slot) {
        uint r=registry+32u+slot*8u;
        if(DC_DATA(r+5u)!=0u && all(equal(x,uvec2(DC_DATA(r),DC_DATA(r+1u)))) &&
           all(equal(y,uvec2(DC_DATA(r+2u),DC_DATA(r+3u))))) return slot+1u;
    }
    return 0u;
}
