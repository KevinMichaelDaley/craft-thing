#ifndef DC_CONTACT_GEOMETRY_REFERENCE_GLSL
#define DC_CONTACT_GEOMETRY_REFERENCE_GLSL
/* Frozen ordered SAT and witness oracle. */
vec2 reference_project_polygon(Polygon polygon, vec2 axis) {
    vec2 range = vec2(dot(polygon.vertices[0], axis));
    for (uint i = 1u; i < polygon.count; ++i) {
        float value = dot(polygon.vertices[i], axis);
        range.x = min(range.x, value); range.y = max(range.y, value);
    }
    return range;
}

bool reference_contact_axis(Polygon a, Polygon b, vec2 edge, inout vec2 normal, inout float depth) {
    vec2 axis = normalize(vec2(-edge.y, edge.x));
    vec2 pa = reference_project_polygon(a, axis), pb = reference_project_polygon(b, axis);
    float positive = pb.y - pa.x, negative = pa.y - pb.x;
    if (positive < -contact_skin || negative < -contact_skin) return false;
    float candidate = max(0.0, min(positive, negative));
    if (candidate < depth) {
        depth = candidate;
        normal = positive <= negative ? axis : -axis;
    }
    return true;
}

bool reference_polygon_contact(Polygon a, Polygon b, out vec2 normal, out float depth) {
    depth = 1e20; normal = vec2(0);
    for (uint i = 0u; i < a.count; ++i)
        if (!reference_contact_axis(a, b, a.vertices[(i + 1u) % a.count] - a.vertices[i], normal, depth))
            return false;
    for (uint i = 0u; i < b.count; ++i)
        if (!reference_contact_axis(a, b, b.vertices[(i + 1u) % b.count] - b.vertices[i], normal, depth))
            return false;
    return true;
}

void reference_support_face(Polygon polygon, vec2 direction, vec2 tangent,
                  out float plane, out vec2 span, out uint feature) {
    plane = reference_project_polygon(polygon, direction).y;
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

void reference_contact_witnesses(Polygon a, Polygon b, vec2 normal,
                       out vec2 point_a, out vec2 point_b, out uint feature_a, out uint feature_b) {
    vec2 tangent = vec2(-normal.y, normal.x), span_a, span_b;
    float plane_a, plane_b;
    reference_support_face(a, -normal, tangent, plane_a, span_a, feature_a);
    reference_support_face(b, normal, tangent, plane_b, span_b, feature_b);
    float low = max(span_a.x, span_b.x), high = min(span_a.y, span_b.y);
    float ta = (span_a.x + span_a.y) * 0.5, tb = (span_b.x + span_b.y) * 0.5;
    if (low <= high) { ta = (low + high) * 0.5; tb = ta; }
    point_a = -normal * plane_a + tangent * ta;
    point_b = normal * plane_b + tangent * tb;
}


#endif

