#ifndef DC_CONTACT_GEOMETRY_GLSL
#define DC_CONTACT_GEOMETRY_GLSL

const float CONTACT_SKIN = 1.0 / 65536.0;
float contact_skin = CONTACT_SKIN;
const float FEATURE_EPSILON = 0.0001;

struct Polygon { vec2 vertices[BODY_VERTEX_CAPACITY]; uint count; };

Polygon body_polygon(Body body,uint piece, vec2 offset) {
    Polygon polygon;
    polygon.count = piece_vertices(body,piece);
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
    for (uint i = 1u; i < polygon.count; ++i) {
        float value = dot(polygon.vertices[i], axis);
        range.x = min(range.x, value); range.y = max(range.y, value);
    }
    return range;
}

bool contact_axis(Polygon a, Polygon b, vec2 edge, inout vec2 normal, inout float depth) {
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

bool polygon_contact(Polygon a, Polygon b, out vec2 normal, out float depth) {
    depth = 1e20; normal = vec2(0);
    for (uint i = 0u; i < a.count; ++i)
        if (!contact_axis(a, b, a.vertices[(i + 1u) % a.count] - a.vertices[i], normal, depth))
            return false;
    for (uint i = 0u; i < b.count; ++i)
        if (!contact_axis(a, b, b.vertices[(i + 1u) % b.count] - b.vertices[i], normal, depth))
            return false;
    return true;
}

void support_face(Polygon polygon, vec2 direction, vec2 tangent,
                  out float plane, out vec2 span, out uint feature) {
    plane = project_polygon(polygon, direction).y;
    span = vec2(1e20, -1e20); feature = 0u;
    bool found = false;
    for (uint i = 0u; i < polygon.count; ++i) {
        if (abs(dot(polygon.vertices[i], direction) - plane) > FEATURE_EPSILON) continue;
        float value = dot(polygon.vertices[i], tangent);
        span.x = min(span.x, value); span.y = max(span.y, value);
        if (!found) { feature = i; found = true; }
    }
    for (uint i = 0u; i < polygon.count; ++i) {
        if (abs(dot(polygon.vertices[i], direction) - plane) <= FEATURE_EPSILON &&
            abs(dot(polygon.vertices[(i + 1u) % polygon.count], direction) - plane) <= FEATURE_EPSILON) {
            feature = 0x80000000u | i;
            break;
        }
    }
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
