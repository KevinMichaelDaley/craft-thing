#ifndef DC_RIGID_BODY_GLSL
#define DC_RIGID_BODY_GLSL

const uint BODY_CAPACITY = 64u;

struct Body {
    int x_fp, y_fp, vx_fp, vy_fp;
    uint width, height, id, enabled;
    int previous_x_fp, previous_y_fp;
    uint reserved0, reserved1;
    uvec2 chunk_x, chunk_y;
    ivec2 local_fp;
    uint visible, reserved_world;
};

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
