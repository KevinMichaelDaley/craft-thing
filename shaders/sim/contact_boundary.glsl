#ifndef DC_CONTACT_BOUNDARY_GLSL
#define DC_CONTACT_BOUNDARY_GLSL
#ifndef DC_CONTACT_COOPERATIVE_BOUNDARY
#define DC_CONTACT_COOPERATIVE_BOUNDARY 1
#endif
/* Every active invocation must query the same body; pieces may differ. */
bool contact_dense_pixel_body(Body body) {
    uint area=pixel_tile_count(body)*256u;
    return (body.piece_count&0x7fffffffu)>area-area/4u;
}
bool contact_pixel_boundary(Body body,uint piece,bool cooperative) {
#if DC_CONTACT_COOPERATIVE_BOUNDARY
    if(!cooperative) return pixel_boundary(body,piece);
    bool valid=piece<pixel_tile_count(body)*256u;
    uint word=piece/8u,own_mask=0u;
    uvec4 remaining=subgroupBallot(valid);
    while(subgroupBallotBitCount(remaining)!=0u) {
        uint leader=subgroupBallotFindLSB(remaining);
        uint key=subgroupBroadcast(word,leader),mask=0u;
        if(gl_SubgroupInvocationID==leader) mask=pixel_boundary_mask(body,key);
        mask=subgroupBroadcast(mask,leader);
        bool matching=valid&&word==key;
        if(matching) own_mask=mask;
        remaining&=~subgroupBallot(matching);
    }
    return valid&&(own_mask&(1u<<((piece%8u)*4u)))!=0u;
#else
    return pixel_boundary(body,piece);
#endif
}
#endif
