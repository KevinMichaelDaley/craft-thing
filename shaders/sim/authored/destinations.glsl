void map_destination() {
    uint i=gl_GlobalInvocationID.x;
    if(i>=pixels() || words[2]!=0u) return;
    uint d=params.destination_offset+16u*i, role=(words[physical(i)]>>16u)&255u;
    uint x=i%params.width,y=i/params.width,rx=x,ry=y;
    uint component=INVALID,slot=INVALID,geometry=INVALID;
    if(role==3u) {
        component=words[label(i)];
        uint record=output_plan(words[plan(component)]);
        slot=words[record+9u]; geometry=words[record+10u];
        rx-=words[record+3u]; ry-=words[record+4u];
    } else if(role==2u) component=words[members(i)];
    words[d]=role; words[d+1u]=words[physical(i)+1u]; words[d+2u]=component; words[d+3u]=slot;
    words[d+4u]=geometry; words[d+5u]=(ry%DC_PIXEL_TILE_SIDE)*DC_PIXEL_TILE_SIDE+rx%DC_PIXEL_TILE_SIDE;
    words[d+6u]=i; words[d+7u]=params.source_frame;
    uint lx=params.local_x+x,ly=params.local_y+y;
    uint high_x,high_y;
    words[d+8u]=uaddCarry(params.chunk_x_lo,lx/64u,high_x);
    words[d+9u]=params.chunk_x_hi+high_x;
    words[d+10u]=uaddCarry(params.chunk_y_lo,ly/64u,high_y);
    words[d+11u]=params.chunk_y_hi+high_y;
    words[d+12u]=lx%64u; words[d+13u]=ly%64u;
    words[d+14u]=rx; words[d+15u]=ry;
}
