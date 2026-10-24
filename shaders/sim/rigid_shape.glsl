#ifndef DC_RIGID_SHAPE_GLSL
#define DC_RIGID_SHAPE_GLSL

const float BODY_FIXED_SCALE = 65536.0;

bool piece_edge_separates_cell(vec2 a,vec2 b,vec2 low,vec2 high) {
    vec2 edge=b-a,inward=vec2(-edge.y,edge.x);
    vec2 support=mix(low,high,greaterThanEqual(inward,vec2(0.0)));
    return dot(inward,support-a)<=0.0;
}
bool piece_cell_overlap(Body body,uint piece, ivec2 cell) {
    vec2 low = vec2((cell << 16) - ivec2(body.x_fp, body.y_fp)) / BODY_FIXED_SCALE;
    vec2 high = low + vec2(1.0);
    if(pixel_body(body)) {
        BodyQuad q;if(!body_pixel_quad(body,piece,q)) return false;
        if(piece_edge_separates_cell(q.a,q.b,low,high)||piece_edge_separates_cell(q.b,q.c,low,high)||
           piece_edge_separates_cell(q.c,q.d,low,high)||piece_edge_separates_cell(q.d,q.a,low,high)) return false;
        vec2 bounds_low=min(min(min(min(vec2(1e20),q.a),q.b),q.c),q.d);
        vec2 bounds_high=max(max(max(max(vec2(-1e20),q.a),q.b),q.c),q.d);
        return all(greaterThan(high,bounds_low))&&all(lessThan(low,bounds_high));
    }
    uint count=piece_vertices(body,piece);if(count==0u) return false;
    vec2 bounds_low = vec2(1e20), bounds_high = vec2(-1e20);
    for (uint i = 0u; i < count; ++i) {
        vec2 a = piece_vertex(body,piece,i);
        vec2 b = piece_vertex(body,piece,(i + 1u) % count);
        bounds_low = min(bounds_low, a); bounds_high = max(bounds_high, a);
        vec2 edge = b - a, inward = vec2(-edge.y, edge.x);
        vec2 support = mix(low, high, greaterThanEqual(inward, vec2(0.0)));
        if (dot(inward, support - a) <= 0.0) return false;
    }
    return all(greaterThan(high, bounds_low)) && all(lessThan(low, bounds_high));
}
bool convex_cell_overlap(Body body,ivec2 cell) {
    vec2 low=vec2(cell)-vec2(body.x_fp,body.y_fp)/65536.;
    uvec4 query=body_piece_query(body,low,low+1);
    for(uint i=0u;i<query.z*query.w;++i) if(piece_cell_overlap(body,query_piece(body,query,i),cell)) return true;
    return false;
}

#endif
