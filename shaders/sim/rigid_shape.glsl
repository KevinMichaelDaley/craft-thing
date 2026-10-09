#ifndef DC_RIGID_SHAPE_GLSL
#define DC_RIGID_SHAPE_GLSL

const float BODY_FIXED_SCALE = 65536.0;

bool piece_cell_overlap(Body body,uint piece, ivec2 cell) {
    vec2 low = vec2((cell << 16) - ivec2(body.x_fp, body.y_fp)) / BODY_FIXED_SCALE;
    vec2 high = low + vec2(1.0);
    vec2 bounds_low = vec2(BODY_MAX_SIDE), bounds_high = vec2(0.0);
    for (uint i = 0u; i < piece_vertices(body,piece); ++i) {
        vec2 a = piece_vertex(body,piece,i);
        vec2 b = piece_vertex(body,piece,(i + 1u) % piece_vertices(body,piece));
        bounds_low = min(bounds_low, a); bounds_high = max(bounds_high, a);
        vec2 edge = b - a, inward = vec2(-edge.y, edge.x);
        vec2 support = mix(low, high, greaterThanEqual(inward, vec2(0.0)));
        if (dot(inward, support - a) <= 0.0) return false;
    }
    return all(greaterThan(high, bounds_low)) && all(lessThan(low, bounds_high));
}
bool convex_cell_overlap(Body body,ivec2 cell) {
    for(uint p=0u;p<body_pieces(body);++p) if(piece_cell_overlap(body,p,cell)) return true;
    return false;
}

#endif
