const uint STRUCTURAL_SOURCE=0x53545231u;
const float STRUCTURAL_LOAD_SCALE=4096.0;
uint structural_base() {
    uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
    return 8u+2u*w*h+DC_LAYOUT_PAIR_CAPACITY*8u+12u+DC_LAYOUT_CONTACT_CAPACITY*32u+16u+64u*32u+DC_LAYOUT_CONTACT_CAPACITY*16u;
}
uint structural_graph() {
    uint s=structural_base();return s+32u+DC_DATA(s+1u)*8u;
}
uint structural_loads() { return structural_graph()+25000u; }
uint structural_bad() { return structural_graph()+25100u; }
uint structural_dependencies() { return structural_graph()+25200u; }
uint structural_clipped() { return structural_graph()+25300u; }
uint body_impact_pending(uint slot) {
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_BODY_STATE(slot)+52u;
#else
    return structural_graph()+38000u+slot*4u;
#endif
}
uint body_impact_ledger(uint slot) {
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_BODY_STATE(slot)+56u;
#else
    return structural_graph()+38500u+slot*4u;
#endif
}
uint body_fragment_record(uint slot) {
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_BODY_STATE(slot)+60u;
#else
    return structural_graph()+36000u+slot*4u;
#endif
}
uint structural_bins() { return structural_graph()+26000u; }
int structural_node(uint id) {
    uint g=structural_graph();
    for(uint i=0u;i<DC_DATA(g);i++) if(DC_DATA(g+16u+i*12u)==id) return int(i);
    return -1;
}
void structural_sum(uint p,float value,uint node) {
    float scaled=round(value*STRUCTURAL_LOAD_SCALE);
    if(isnan(scaled)||isinf(scaled)||abs(scaled)>1073741823.0) {
        atomicOr(DC_DATA(structural_bad()+node),1u);return;
    }
    int term=int(scaled),previous=int(atomicAdd(DC_DATA(p),uint(term)));
    if((term>0&&previous>2147483647-term)||(term<0&&previous<(-2147483647-1)-term))
        atomicOr(DC_DATA(structural_bad()+node),1u);
}
void structural_capture(uint source,uint other,vec2 point,vec2 impulse) {
    if(DC_DATA(structural_base())==0u||source>=BODY_CAPACITY||
       DC_BODY(source).reserved0!=STRUCTURAL_SOURCE) return;
    uint member=DC_BODY(source).reserved1-1u,g=structural_graph(),m=g+400u+member*8u;
    int a=structural_node(DC_DATA(m+2u)),b=structural_node(DC_DATA(m+3u));
    if(a<0||b<0) return;
    Body body=DC_BODY(source);vec2 d=body_rotate(vec2(1,0),body.angle),t=vec2(-d.y,d.x);
    /* Rest endpoints are the shape's centerline, not its raster AABB. */
    float l=float(body.vertices[1].x)/65536.0,depth=float(body.vertices[2].y)/65536.0;
    vec2 center=vec2(body.x_fp,body.y_fp)/65536.0+body.center;
    vec2 first=center-d*l/2.0;
    float r=clamp(dot(point-first,d)/l,0,1),couple=body_cross(point-(first+r*l*d),impulse);
    float axial=dot(impulse,d),transverse=dot(impulse,t);
    uint p=g+29000u+DC_DATA(m+4u)*12u;
    float young=uintBitsToFloat(DC_DATA(p+1u)),shear=uintBitsToFloat(DC_DATA(p+2u));
    float phi=young*depth*depth/((5.0/6.0)*shear*l*l),den=1+phi;
    vec4 h=vec4(1-3*r*r+2*r*r*r+phi*(1-r),
        l*(r-2*r*r+r*r*r+phi*r*(1-r)/2),
        3*r*r-2*r*r*r+phi*r,
        l*(-r*r+r*r*r-phi*r*(1-r)/2))/den;
    vec4 rotation=vec4(6*r*(r-1)/l,1-4*r+3*r*r+phi*(1-r),
        6*r*(1-r)/l,-2*r+3*r*r+phi*r)/den;
    vec2 fa=(1-r)*axial*d+(h.x*transverse+rotation.x*couple)*t;
    vec2 fb=r*axial*d+(h.z*transverse+rotation.z*couple)*t;
    float ma=h.y*transverse+rotation.y*couple,mb=h.w*transverse+rotation.w*couple;
    structural_sum(structural_loads()+uint(a)*3u,fa.x,uint(a));
    structural_sum(structural_loads()+uint(a)*3u+1u,fa.y,uint(a));
    structural_sum(structural_loads()+uint(a)*3u+2u,ma,uint(a));
    structural_sum(structural_loads()+uint(b)*3u,fb.x,uint(b));
    structural_sum(structural_loads()+uint(b)*3u+1u,fb.y,uint(b));
    structural_sum(structural_loads()+uint(b)*3u+2u,mb,uint(b));
    /* Position bins preserve total force and first moment for stress recovery. */
    float bin=r*16.0;uint low=min(uint(floor(bin)),15u);float fraction=bin-float(low);
    vec3 load=vec3(axial,transverse,couple);
    for(uint axis=0u;axis<3u;axis++) {
        structural_sum(structural_bins()+(member*17u+low)*3u+axis,(1-fraction)*load[axis],uint(a));
        structural_sum(structural_bins()+(member*17u+low+1u)*3u+axis,fraction*load[axis],uint(a));
    }
    if(other<BODY_CAPACITY) atomicOr(DC_BODY_DEPENDENCIES(other),(1u<<uint(a))|(1u<<uint(b)));
}
