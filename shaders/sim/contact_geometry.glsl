#ifndef DC_CONTACT_GEOMETRY_GLSL
#define DC_CONTACT_GEOMETRY_GLSL

const float CONTACT_SKIN = 1.0 / 65536.0;
float contact_skin = CONTACT_SKIN;
const float FEATURE_EPSILON = 0.0001;

#ifndef DC_CONTACT_POLYGON_CAPACITY
#define DC_CONTACT_POLYGON_CAPACITY BODY_VERTEX_CAPACITY
#endif
struct Polygon { vec2 vertices[DC_CONTACT_POLYGON_CAPACITY]; uint count; };

Polygon body_polygon(Body body,uint piece, vec2 offset) {
    Polygon polygon;
    /* Typed contacts call this only after the occupied boundary test. Keeping
       that proven count constant avoids dynamic polygon loops for pixel cells. */
    polygon.count = CONTACT_PIXEL_ONLY ? 4u : piece_vertices(body,piece);
    if(CONTACT_PIXEL_ONLY) {
        vec2 cell=vec2(pixel_piece_cell(body,piece));
        for(uint i=0u;i<4u;i++) {
            vec2 local=cell+vec2((i==1u||i==2u)?1:0,i>=2u?1:0);
            polygon.vertices[i]=offset+(body.center+body_rotate(local-body.center,body.angle));
        }
        return polygon;
    }
    for (uint i = 0u; i < polygon.count; ++i)
        polygon.vertices[i] = offset + piece_vertex(body,piece,i);
    return polygon;
}

Polygon cell_polygon(vec2 low) {
    Polygon polygon;
    polygon.count = 4u;
    polygon.vertices[0] = low;
    polygon.vertices[1] = low + vec2(1, 0);
    polygon.vertices[2] = low + vec2(1, 1);
    polygon.vertices[3] = low + vec2(0, 1);
    return polygon;
}

vec2 project_polygon(Polygon polygon, vec2 axis) {
    vec2 range = vec2(dot(polygon.vertices[0], axis));
    if(CONTACT_PIXEL_ONLY&&polygon.count==4u) {
        float value=dot(polygon.vertices[1],axis);
        range=vec2(min(range.x,value),max(range.y,value));
        value=dot(polygon.vertices[2],axis);
        range=vec2(min(range.x,value),max(range.y,value));
        value=dot(polygon.vertices[3],axis);
        return vec2(min(range.x,value),max(range.y,value));
    }
    for (uint i = 1u; i < polygon.count; ++i) {
#ifdef DC_CONTACT_PROFILE
        contact_dynamic_iterations++;
#endif
        float value = dot(polygon.vertices[i], axis);
        range.x = min(range.x, value); range.y = max(range.y, value);
    }
    return range;
}

bool contact_axis(Polygon a, Polygon b, vec2 edge, inout vec2 normal, inout float depth) {
#ifdef DC_CONTACT_PROFILE
    contact_axis_invocations++;
#endif
    vec2 axis = normalize(vec2(-edge.y, edge.x));
    vec2 pa = project_polygon(a, axis), pb = project_polygon(b, axis);
    float positive = pb.y - pa.x, negative = pa.y - pb.x;
    if (positive < -contact_skin || negative < -contact_skin) return false;
    float candidate = max(0.0, min(positive, negative));
    if (candidate < depth) {
        depth = candidate;
        normal = positive <= negative ? axis : -axis;
    }
    return true;
}

bool polygon_axis_rectangle(Polygon p) {
    return p.vertices[0].y==p.vertices[1].y&&p.vertices[1].x==p.vertices[2].x&&
        p.vertices[2].y==p.vertices[3].y&&p.vertices[3].x==p.vertices[0].x&&
        p.vertices[1].x>p.vertices[0].x&&p.vertices[2].y>p.vertices[1].y;
}
bool quad_contact_axes(Polygon a, Polygon b, Polygon edges, inout vec2 normal, inout float depth) {
    bool rectangle = polygon_axis_rectangle(edges);
    for (uint i = 0u; i < 4u; ++i) {
        if (i == 2u && rectangle) break;
        vec2 edge = edges.vertices[(i + 1u) % 4u] - edges.vertices[i];
        if (!contact_axis(a, b, edge, normal, depth)) return false;
    }
    return true;
}

bool polygon_contact(Polygon a, Polygon b, out vec2 normal, out float depth) {
    depth = 1e20; normal = vec2(0);
    if (CONTACT_PIXEL_ONLY && a.count == 4u && b.count == 4u)
        return quad_contact_axes(a, b, a, normal, depth) && quad_contact_axes(a, b, b, normal, depth);
    for (uint i = 0u; i < a.count; ++i)
        if (!contact_axis(a, b, a.vertices[(i + 1u) % a.count] - a.vertices[i], normal, depth))
            return false;
    for (uint i = 0u; i < b.count; ++i)
        if (!contact_axis(a, b, b.vertices[(i + 1u) % b.count] - b.vertices[i], normal, depth))
            return false;
    return true;
}

void ordered_support_face(Polygon polygon, uint count, vec2 direction, vec2 tangent,
                          out float plane, out vec2 span, out uint feature) {
    plane = dot(polygon.vertices[0], direction);
    for (uint i = 1u; i < count; ++i)
        plane = max(plane, dot(polygon.vertices[i], direction));
    span = vec2(1e20, -1e20); feature = 0u;
    bool found = false;
    for (uint i = 0u; i < count; ++i) {
        if (abs(dot(polygon.vertices[i], direction) - plane) > FEATURE_EPSILON) continue;
        float value = dot(polygon.vertices[i], tangent);
        span.x = min(span.x, value); span.y = max(span.y, value);
        if (!found) { feature = i; found = true; }
    }
    for (uint i = 0u; i < count; ++i) {
        if (abs(dot(polygon.vertices[i], direction) - plane) <= FEATURE_EPSILON &&
            abs(dot(polygon.vertices[(i + 1u) % count], direction) - plane) <= FEATURE_EPSILON) {
            feature = 0x80000000u | i;
            break;
        }
    }
}

void support_face(Polygon polygon, vec2 direction, vec2 tangent,
                  out float plane, out vec2 span, out uint feature) {
    if (CONTACT_PIXEL_ONLY && polygon.count == 4u)
        ordered_support_face(polygon, 4u, direction, tangent, plane, span, feature);
    else
        ordered_support_face(polygon, polygon.count, direction, tangent, plane, span, feature);
}

void contact_witnesses(Polygon a, Polygon b, vec2 normal,
                       out vec2 point_a, out vec2 point_b, out uint feature_a, out uint feature_b) {
    vec2 tangent = vec2(-normal.y, normal.x), span_a, span_b;
    float plane_a, plane_b;
    support_face(a, -normal, tangent, plane_a, span_a, feature_a);
    support_face(b, normal, tangent, plane_b, span_b, feature_b);
    float low = max(span_a.x, span_b.x), high = min(span_a.y, span_b.y);
    float ta = (span_a.x + span_a.y) * 0.5, tb = (span_b.x + span_b.y) * 0.5;
    if (low <= high) { ta = (low + high) * 0.5; tb = ta; }
    point_a = -normal * plane_a + tangent * ta;
    point_b = normal * plane_b + tangent * tb;
}

#endif
