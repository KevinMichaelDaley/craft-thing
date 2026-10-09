#ifndef DC_RIGID_BODY_GLSL
#define DC_RIGID_BODY_GLSL

const uint BODY_CAPACITY = 64u;
const uint BODY_VERTEX_CAPACITY = 8u;
const uint BODY_MAX_SIDE = 16u;
const uint BODY_PIECES = 4u;
struct Shape { uint count, material; ivec2 vertices[BODY_VERTEX_CAPACITY]; };

struct Body {
    int x_fp, y_fp, vx_fp, vy_fp;
    uint width, height, id, enabled;
    int previous_x_fp, previous_y_fp;
    uint reserved0, reserved1;
    uvec2 chunk_x, chunk_y;
    ivec2 local_fp;
    uint visible;
    float radius;
    uint vertex_count, material;
    ivec2 vertices[BODY_VERTEX_CAPACITY];
    float angle, angular_velocity;
    uint motion_flags, motion_ready;
    vec2 center;
    float inverse_mass, inverse_inertia, mass, inertia, density, previous_angle;
    ivec2 bounds_low, bounds_high, swept_low, swept_high;
    uint piece_count, compound_reserved;
    Shape pieces[BODY_PIECES];
};

vec2 body_rotate(vec2 p, float angle) {
    float c = cos(angle), s = sin(angle);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}
float body_cross(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }
uint body_vertices(Body b) { return b.vertex_count == 0u ? 4u : b.vertex_count; }
vec2 body_rest_vertex(Body b, uint i) {
    if (b.vertex_count != 0u) return vec2(b.vertices[i]) / 65536.0;
    return vec2((i == 1u || i == 2u) ? b.width : 0u, i >= 2u ? b.height : 0u);
}
vec2 body_vertex(Body b, uint i) {
    return b.center + body_rotate(body_rest_vertex(b, i) - b.center, b.angle);
}
uint body_pieces(Body b) { return max(1u,b.piece_count); }
uint piece_vertices(Body b,uint p) { return b.piece_count==0u ? body_vertices(b) : b.pieces[p].count; }
vec2 piece_rest_vertex(Body b,uint p,uint i) {
    return b.piece_count==0u ? body_rest_vertex(b,i) : vec2(b.pieces[p].vertices[i])/65536.0;
}
vec2 piece_vertex(Body b,uint p,uint i) {
    return b.center+body_rotate(piece_rest_vertex(b,p,i)-b.center,b.angle);
}
void body_bounds(Body b, out vec2 low, out vec2 high) {
    low = vec2(1e20); high = vec2(-1e20);
    for(uint piece=0u;piece<body_pieces(b);++piece)
        for (uint i = 0u; i < piece_vertices(b,piece); ++i) {
            vec2 p = piece_vertex(b,piece,i); low = min(low,p); high = max(high,p);
        }
}
void piece_properties(Body b,uint piece,out float area,out vec2 center,out float inertia) {
    vec2 first=piece_rest_vertex(b,piece,0u),weighted=vec2(0);
    float moment=0.0;area=0.0;
    for(uint i=1u;i+1u<piece_vertices(b,piece);++i) {
        vec2 p=piece_rest_vertex(b,piece,i)-first,q=piece_rest_vertex(b,piece,i+1u)-first;
        float a=body_cross(p,q)*0.5;
        area+=a;weighted+=a*(p+q)/3.0;
        moment+=a*(dot(p,p)+dot(p,q)+dot(q,q))/6.0;
    }
    vec2 local_center=weighted/max(area,1e-12);
    center=first+local_center;
    inertia=max(moment-area*dot(local_center,local_center),1e-24);
}
void body_properties(inout Body b) {
    if (b.motion_ready != 0u) return;
    b.density = b.material == 2u ? 0.6 : 2.7;
    vec2 weighted=vec2(0);float area=0.0;
    float areas[BODY_PIECES],inertias[BODY_PIECES];vec2 centers[BODY_PIECES];
    for(uint piece=0u;piece<body_pieces(b);++piece) {
        piece_properties(b,piece,areas[piece],centers[piece],inertias[piece]);
        area+=areas[piece];weighted+=areas[piece]*centers[piece];
    }
    b.center=weighted/max(area,1e-12);
    b.mass = max(area*b.density,1e-12);
    float inertia=0.0;
    for(uint piece=0u;piece<body_pieces(b);++piece) {
        vec2 delta=centers[piece]-b.center;
        inertia+=inertias[piece]+areas[piece]*dot(delta,delta);
    }
    b.inertia=max(b.density*inertia,1e-24);
    b.inverse_mass = (b.motion_flags & 1u) == 0u ? 1.0/b.mass : 0.0;
    b.inverse_inertia = (b.motion_flags & 3u) == 0u ? 1.0/b.inertia : 0.0;
    float radius=0.0;
    for(uint piece=0u;piece<body_pieces(b);++piece)
        for(uint i=0u;i<piece_vertices(b,piece);++i)
            radius=max(radius,length(piece_rest_vertex(b,piece,i)-b.center));
    b.radius=radius;
    b.motion_ready = 1u;
}

void body_cache_bounds(inout Body b) {
    vec2 low,high;body_bounds(b,low,high);
    ivec2 current=ivec2(b.x_fp,b.y_fp),previous=ivec2(b.previous_x_fp,b.previous_y_fp);
    b.bounds_low=(current+ivec2(floor(low*65536.0)))>>16;
    b.bounds_high=(current+ivec2(ceil(high*65536.0))+65535)>>16;
    b.swept_low=min(current,previous)>>16;
    b.swept_high=(max(current,previous)+(ivec2(b.width,b.height)<<16)+
        (b.vertex_count!=0u ? 65535 : 0))>>16;
    if(b.angle!=0.0 || b.previous_angle!=0.0) {
        float radius=b.radius;
        b.swept_low=(min(current,previous)+ivec2(floor((b.center-radius)*65536.0)))>>16;
        b.swept_high=(max(current,previous)+ivec2(ceil((b.center+radius)*65536.0))+65535)>>16;
    }
}

bool relative_chunk(uvec2 anchor, uvec2 origin, out int offset) {
    uvec2 delta = uvec2(anchor.x - origin.x,
        anchor.y - origin.y - uint(anchor.x < origin.x));
    uint sign_a = anchor.y >> 31, sign_b = origin.y >> 31, sign_d = delta.y >> 31;
    offset = int(delta.x);
    return !(sign_a != sign_b && sign_a != sign_d) &&
        delta.y == (offset < 0 ? 0xffffffffu : 0u);
}

bool shifted_chunk(uvec2 origin, int offset, out uvec2 anchor) {
    uint low = origin.x + uint(offset);
    anchor = uvec2(low, origin.y + (offset < 0 ? 0xffffffffu : 0u) +
                   uint(low < origin.x));
    uint sign_o = origin.y >> 31, sign_a = anchor.y >> 31;
    return !((offset < 0 ? 1u : 0u) == sign_o && sign_a != sign_o);
}

#endif
