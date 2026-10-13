#ifndef DC_RIGID_CLIP_GLSL
#define DC_RIGID_CLIP_GLSL
float body_cell_volume(Body b,uint piece,ivec2 cell,float fraction,out vec2 center_point) {
    vec2 polygon[12],next[12];uint count=piece_vertices(b,piece);
    if(count<3u) {center_point=vec2(0);return 0;}
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0;
    for(uint i=0u;i<count;i++) polygon[i]=origin+piece_vertex(b,piece,i);
    vec2 low=vec2(cell)+vec2(0,1-fraction),high=vec2(cell)+1;
    for(uint plane=0u;plane<4u;plane++) {
        uint axis=plane/2u,n=0u;bool upper=(plane&1u)!=0u;
        float boundary=upper?high[axis]:low[axis];
        for(uint i=0u;i<count;i++) {
            vec2 a=polygon[i],z=polygon[(i+1u)%count];
            float da=upper?boundary-a[axis]:a[axis]-boundary;
            float dz=upper?boundary-z[axis]:z[axis]-boundary;
            if(da>=0) next[n++]=a;
            if((da>0&&dz<0)||(da<0&&dz>0)) next[n++]=mix(a,z,da/(da-dz));
        }
        count=n;for(uint i=0u;i<count;i++) polygon[i]=next[i];
        if(count<3u) {center_point=vec2(0);return 0;}
    }
    float twice=0;vec2 moment=vec2(0);
    for(uint i=0u;i<count;i++) {
        vec2 a=polygon[i]-vec2(cell),z=polygon[(i+1u)%count]-vec2(cell);
        float cross=body_cross(a,z);twice+=cross;moment+=(a+z)*cross;
    }
    center_point=vec2(cell)+moment/max(3*twice,1e-20);
    return max(twice*.5,0);
}
float body_face_coverage(Body b,uint piece,vec2 start,vec2 stop) {
    if(piece_vertices(b,piece)==0u) return 0;
    float low=0,high=1;vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0;
    for(uint i=0u;i<piece_vertices(b,piece);i++) {
        vec2 a=origin+piece_vertex(b,piece,i);
        vec2 z=origin+piece_vertex(b,piece,(i+1u)%piece_vertices(b,piece));
        vec2 edge=z-a,normal=vec2(-edge.y,edge.x);
        float first=dot(normal,start-a),delta=dot(normal,stop-start);
        if(abs(delta)<1e-12) {if(first<0) return 0;}
        else if(delta>0) low=max(low,-first/delta);
        else high=min(high,-first/delta);
    }
    return max(0,high-low);
}
#endif
