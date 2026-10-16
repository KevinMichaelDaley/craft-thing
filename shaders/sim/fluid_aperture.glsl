float cached_path_aperture(ivec2 source,ivec2 direction,uint distance) {
    if(params.unused1==0u) return face_aperture(source,direction);
    bool vertical=direction.y!=0;
    int coordinate=vertical?source.y:source.x;
    int delta=vertical?direction.y:direction.x;
    uint low=uint(coordinate+(delta<0?-int(distance):0)),high=low+distance;
    uint first=low/16u,last=(high-1u)/16u;
    if(first==last) {
        float minimum=1.;
        for(uint edge=0u;edge<distance;edge++)
            minimum=min(minimum,face_aperture(source+direction*int(edge),direction));
        return minimum;
    }
    uint line=uint(vertical?source.x:source.y);
    uint left=vertical?low*params.width+line:line*params.width+low;
    uint right=vertical?(high-1u)*params.width+line:line*params.width+high-1u;
    uint minimum=min(aperture_prefix[2u*left+1u],aperture_prefix[2u*right]);
    uint begin=params.unused1+first+1u,end=params.unused1+last;
    uint base=16448u+line*2u*params.unused1;
    while(begin<end) {
        if((begin&1u)!=0u) minimum=min(minimum,aperture_tree[base+begin++]);
        if((end&1u)!=0u) minimum=min(minimum,aperture_tree[base+--end]);
        begin/=2u;end/=2u;
    }
    return float(minimum)/65536.;
}
