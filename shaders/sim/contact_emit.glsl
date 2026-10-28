#ifndef DC_CONTACT_EMIT_GLSL
#define DC_CONTACT_EMIT_GLSL
layout(constant_id=34) const bool RECTANGLE_PAIR_CONTACTS=false;
const uint RECTANGLE_LOW_ENDPOINT=0xffffffffu,RECTANGLE_HIGH_ENDPOINT=0xfffffffeu;
uint contact_feature(Body body,uint piece,uint feature) {
    return pixel_body(body)?(feature&0x80000007u)|(piece<<3u):feature|(piece<<28u);
}

bool filled_rectangle(Body body) {
    if(!RECTANGLE_PAIR_CONTACTS||!pixel_body(body)) return false;
    uvec2 extent=uvec2(pixel_high(body)-pixel_low(body));
    uint count=body.piece_count&0x7fffffffu;
    /* Bounds contain all occupied cells; equal area proves there are no holes. */
    return extent.y!=0u&&extent.x==count/extent.y&&count%extent.y==0u;
}
Polygon rectangle_polygon(Body body,vec2 offset) {
    Polygon p;p.count=4u;vec2 low=pixel_low(body),high=pixel_high(body);
    for(uint i=0u;i<4u;i++) {
        vec2 rest=vec2(i==1u||i==2u?high.x:low.x,i>=2u?high.y:low.y);
        p.vertices[i]=offset+body.center+body_rotate(rest-body.center,body.angle);
    }
    return p;
}
uint rectangle_witness_piece(Body body,vec2 point,vec2 offset) {
    vec2 rest=body.center+body_rotate(point-offset-body.center,-body.angle);
    ivec2 cell=clamp(ivec2(floor(rest)),ivec2(pixel_low(body)),ivec2(pixel_high(body))-1);
    return pixel_piece_at(body,cell);
}

bool emit_contact(Body a, uint body_b, uint kind, uint material_b, uint cell_feature,
                  uvec2 feature_x, uvec2 feature_y, Polygon pa, Polygon pb,uint piece_a,uint piece_b,uint particle_ref) {
    vec2 normal; float depth;
    if (!polygon_contact(pa, pb, normal, depth)) return false;
    bool outer_face=RECTANGLE_PAIR_CONTACTS&&piece_a>=RECTANGLE_HIGH_ENDPOINT&&piece_b>=RECTANGLE_HIGH_ENDPOINT;
    if(pixel_body(a)&&!outer_face) {
        vec2 outward=body_rotate(-normal,-a.angle);ivec2 p=pixel_piece_cell(a,piece_a);
        bool exposed=(abs(outward.x)>1e-5&&pixel_material(a,p+ivec2(outward.x>0?1:-1,0))==0u)||
            (abs(outward.y)>1e-5&&pixel_material(a,p+ivec2(0,outward.y>0?1:-1))==0u);
        if(!exposed) return false;
    }
    uint other_slot=kind==BODY_CONTACT?body_slot(body_b):BODY_CAPACITY;
    Body other=a;
    if(other_slot<BODY_CAPACITY) {
        other=DC_BODY(other_slot);
        if(pixel_body(other)&&!outer_face) {
            vec2 outward=body_rotate(normal,-other.angle);ivec2 p=pixel_piece_cell(other,piece_b);
            bool exposed=(abs(outward.x)>1e-5&&pixel_material(other,p+ivec2(outward.x>0?1:-1,0))==0u)||
                (abs(outward.y)>1e-5&&pixel_material(other,p+ivec2(0,outward.y>0?1:-1))==0u);
            if(!exposed) return false;
        }
    }
    vec2 point_a, point_b; uint feature_a, feature_b;
    contact_witnesses(pa, pb, normal, point_a, point_b, feature_a, feature_b);
    if(outer_face) {
        vec2 tangent=vec2(-normal.y,normal.x),span_a,span_b;float plane_a,plane_b;uint unused;
        support_face(pa,-normal,tangent,plane_a,span_a,unused);
        support_face(pb,normal,tangent,plane_b,span_b,unused);
        float low=max(span_a.x,span_b.x),high=min(span_a.y,span_b.y);
        if(piece_a==RECTANGLE_HIGH_ENDPOINT&&high-low<=FEATURE_EPSILON) return false;
        if(low<=high) {
            float t=piece_a==RECTANGLE_LOW_ENDPOINT?low:high;
            point_a=-normal*plane_a+tangent*t;point_b=normal*plane_b+tangent*t;
        }
        vec2 delta;relative_body(a,other,delta);
        piece_a=rectangle_witness_piece(a,point_a,vec2(0));
        piece_b=rectangle_witness_piece(other,point_b,delta);
        material_b=piece_material(other,piece_b);
    }
    uvec2 ax, ay, bx, by; ivec2 alocal, blocal;
    uint base = contact_base();
    if (!world_point(a, point_a, ax, ay, alocal) || !world_point(a, point_b, bx, by, blocal)) {
        atomicOr(DC_DATA(base + 2u), 2u);
        return true;
    }
    uint index = atomicAdd(DC_DATA(base + 1u), 1u);
    if (index >= DC_DATA(base + 3u)) {
        atomicOr(DC_DATA(base + 2u), 1u);
#ifdef DC_RUNTIME_BODY_STORAGE
        if(DC_DATA(base+3u)==DC_LAYOUT_CONTACT_CAPACITY) atomicMax(DC_RUNTIME_REQUEST(7u),index+1u);
#endif
        return true;
    }
    uint offset = base + CONTACT_HEADER_WORDS + index * CONTACT_WORDS;
    uint material_a = piece_material(a,piece_a);
    if(material_a==0u) material_a=1u;
    float fa = body_friction(material_a);
    float fb = kind == BODY_CONTACT ? body_friction(material_b) : cell_friction(material_b);
    float restitution_a = material_a == 2u ? 0.1 : 0.05;
    float restitution_b = kind == BODY_CONTACT && material_b == 2u ? 0.1 : 0.05;
    float compliance = kind == MPM_CONTACT ? 0.0001 :
        (material_a == 2u || (kind == BODY_CONTACT && material_b == 2u) ? 0.00002 : 0.0);
    DC_DATA(offset) = a.id; DC_DATA(offset + 1u) = body_b; DC_DATA(offset + 2u) = kind;
    DC_DATA(offset + 3u) = contact_feature(a,piece_a,feature_a);
    DC_DATA(offset + 4u) = kind == BODY_CONTACT ? contact_feature(other,piece_b,feature_b) : cell_feature;
    DC_DATA(offset + 5u) = material_a; DC_DATA(offset + 6u) = material_b; DC_DATA(offset + 7u) = particle_ref;
    DC_DATA(offset + 8u) = floatBitsToUint(normal.x); DC_DATA(offset + 9u) = floatBitsToUint(normal.y);
    DC_DATA(offset + 10u) = floatBitsToUint(depth); DC_DATA(offset + 11u) = floatBitsToUint(sqrt(fa * fb));
    DC_DATA(offset + 12u) = floatBitsToUint(min(restitution_a, restitution_b));
    DC_DATA(offset + 13u) = floatBitsToUint(compliance); DC_DATA(offset + 14u) = 0u; DC_DATA(offset + 15u) = 0u;
    DC_DATA(offset + 16u) = ax.x; DC_DATA(offset + 17u) = ax.y;
    DC_DATA(offset + 18u) = ay.x; DC_DATA(offset + 19u) = ay.y;
    DC_DATA(offset + 20u) = uint(alocal.x); DC_DATA(offset + 21u) = uint(alocal.y);
    DC_DATA(offset + 22u) = bx.x; DC_DATA(offset + 23u) = bx.y;
    DC_DATA(offset + 24u) = by.x; DC_DATA(offset + 25u) = by.y;
    DC_DATA(offset + 26u) = uint(blocal.x); DC_DATA(offset + 27u) = uint(blocal.y);
    DC_DATA(offset + 28u) = feature_x.x; DC_DATA(offset + 29u) = feature_x.y;
    DC_DATA(offset + 30u) = feature_y.x; DC_DATA(offset + 31u) = feature_y.y;
    atomicAdd(DC_DATA(base), 1u);
    if(particle_ref!=0u) atomicAdd(DC_DATA(base+CONTACT_HEADER_WORDS+DC_LAYOUT_CONTACT_CAPACITY*CONTACT_WORDS+13u),1u);
    return true;
}

bool emit_contact(Body a,uint body_b,uint kind,uint material_b,uint cell_feature,
    uvec2 feature_x,uvec2 feature_y,Polygon pa,Polygon pb,uint piece_a,uint piece_b) {
    return emit_contact(a,body_b,kind,material_b,cell_feature,feature_x,feature_y,
        pa,pb,piece_a,piece_b,0u);
}
#endif
