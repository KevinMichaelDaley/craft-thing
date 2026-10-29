/* Timers use integer fixed physics ticks, never wall time or fluid step duration.
 * Sound members reuse their reserved collision record without a second copy. */
const uint STRUCTURAL_RETIRED=2u;
#include "structural_bonds.glsl"
uint failure_clock(uint node) { return graph()+28000u+node*4u; }
bool member_active(uint member) {
    return (DC_DATA(graph()+MEMBERS+member*8u+6u)&STRUCTURAL_RETIRED)==0u;
}
bool component_member(uint member,uint id) {
    uint m=graph()+MEMBERS+member*8u;
    int a=node_index(graph(),DC_DATA(m+2u));
    return member_active(member)&&DC_DATA(state(uint(a))+1u)==id;
}
bool shared_node(uint a,uint b,out uint node) {
    uint g=graph(),ma=g+MEMBERS+a*8u,mb=g+MEMBERS+b*8u;
    for(uint x=2u;x<4u;x++) for(uint y=2u;y<4u;y++)
        if(DC_DATA(ma+x)==DC_DATA(mb+y)) {node=uint(node_index(g,DC_DATA(ma+x)));return true;}
    return false;
}
int member_at(uint node,uint component) {
    for(uint m=0u;m<DC_DATA(graph()+1u);m++) if(component_member(m,component)) {
        uint p=graph()+MEMBERS+m*8u;
        if(DC_DATA(p+2u)==node||DC_DATA(p+3u)==node) return int(m);
    }
    return -1;
}
void create_bond(uint a,uint b,vec2 point,uint kind,vec3 stiffness,float normal,
                 float cohesion,float friction) {
    uint slot=0u;
    while(slot<STRUCTURAL_BOND_CAPACITY&&bond_live(bond_record(slot))) slot++;
    uint p=bond_record(slot);Body ba=DC_BODY(a),bb=DC_BODY(b);
    for(uint word=0u;word<STRUCTURAL_BOND_WORDS;word++) DC_DATA(p+word)=0u;
    DC_DATA(p)=1u;DC_DATA(p+1u)=a;DC_DATA(p+2u)=b;DC_DATA(p+3u)=ba.id;DC_DATA(p+4u)=bb.id;DC_DATA(p+5u)=kind;
    vec2 pa=body_rotate(point-vec2(ba.x_fp,ba.y_fp)/65536.0-ba.center,-ba.angle);
    vec2 pb=body_rotate(point-vec2(bb.x_fp,bb.y_fp)/65536.0-bb.center,-bb.angle);
    for(uint axis=0u;axis<2u;axis++) {
        DC_DATA(p+6u+axis)=floatBitsToUint(pa[axis]);DC_DATA(p+8u+axis)=floatBitsToUint(pb[axis]);
    }
    DC_DATA(p+10u)=floatBitsToUint(bb.angle-ba.angle);
    DC_DATA(p+11u)=floatBitsToUint(normal-ba.angle);
    for(uint axis=0u;axis<3u;axis++) DC_DATA(p+12u+axis)=floatBitsToUint(stiffness[axis]);
    DC_DATA(p+18u)=floatBitsToUint(cohesion);DC_DATA(p+19u)=floatBitsToUint(friction);
    DC_DATA(base()+9u)=1u;DC_DATA(base()+10u)=1u;DC_DATA(base()+11u)=1u;
}
void release_member(uint member) {
    uint g=graph(),m=g+MEMBERS+member*8u,slot=DC_DATA(m+7u);
    Body b=DC_BODY(slot);
    b.motion_flags=0u;b.motion_ready=0u;b.reserved0=0x53545232u;b.reserved1=DC_DATA(m+1u);
    b.vx_fp=0;b.vy_fp=0;b.angular_velocity=0;
    b.rest_ticks=0u;body_properties(b);DC_BODY(slot)=b;
    DC_DATA(m+6u)|=STRUCTURAL_RETIRED;
    DC_DATA(g+24000u+member*16u+3u)=5u;
}
#include "structural_debris.glsl"
#include "rigid_fragment.glsl"
#include "pixel_fragment.glsl"
float member_wood_section(uint slot) {
#ifdef DC_RUNTIME_BODY_STORAGE
    if(slot>=BODY_CAPACITY) return 1.0;
    Body body=DC_BODY(slot);
    if(body.material!=2u||(body.motion_flags&DC_BODY_MOTION_THERMAL_GRID)==0u||
       !reactive_enabled()) return 1.0;
    Body grid=body_thermal_grid(body);
    float section=1.0;
    for(uint pixel=0u;pixel<body_pieces(grid);pixel++) {
        if(pixel_piece_material(grid,pixel)==0u) continue;
        uint word=grid.compound_reserved+pixel_tile_offset(grid,pixel/256u)+
                  1u+(pixel%256u)/8u;
        uvec2 handle=reactive_shape_read(word-color_pool_word(),pixel%8u,
                                         color_capacity());
        if(!reactive_handle_valid(handle)) continue;
        ReactiveWords parcel=reactive_entry_buffer(handle.x);
        uint state=FR_ENTRY_STATE;
        if(parcel.words[FR_ENTRY_OWNER]!=2u||
           parcel.words[FR_ENTRY_OWNER+1u]!=body.id||
           parcel.words[state+8u]==0u) continue;
        float original=float(parcel.words[state+8u]);
        float fuel=float(parcel.words[state+9u]);
        float char_mass=float(parcel.words[state+10u]);
        float inert=float(parcel.words[state+11u]);
        float water=float(parcel.words[state+12u]);
        float capacity=((fuel+inert)*1.5+char_mass+water*4.18)/65536.0;
        float energy=uintBitsToFloat(parcel.words[state+17u])+
                     uintBitsToFloat(parcel.words[state+25u])+
                     uintBitsToFloat(parcel.words[state+26u]);
        float temperature=capacity>0.0?energy/capacity:293.15;
        float heat_strength=1.0-.3*clamp((temperature-400.0)/200.0,0.0,1.0);
        float load_bearing=fuel+.35*char_mass;
        section=min(section,clamp(load_bearing/original,0.0,1.0)*heat_strength);
    }
    return section;
#else
    return 1.0;
#endif
}
void damage_members() {
    uint g=graph();bool cut=false;
    for(uint member=0u;member<DC_DATA(g+1u);member++) if(member_active(member)) {
        uint m=g+MEMBERS+member*8u,st=member_state(member);
        if(DC_DATA(st+3u)>=2u) continue;
        uint slot=DC_DATA(m+7u);
        float section=member_wood_section(slot);
        float excess=max(eq_get(st+7u)/max(section,.05)-1.0,0);
        uint profile=g+29000u+DC_DATA(m+4u)*12u;
        float damage=min(1.0,max(eq_get(st+10u),1.0-section)+
                         excess*excess/(60.0*eq_get(profile+8u)));
        eq_put(st+10u,damage);
        if(damage<1.0) continue;
        if(slot>=BODY_CAPACITY||DC_BODY(slot).reserved0!=STRUCTURAL_SOURCE||
           DC_BODY(slot).reserved1!=member+1u||DC_BODY(slot).enabled==0u) {
            DC_DATA(st+3u)=4u;continue;
        }
        float compression=max(-eq_get(st+4u),0)/(float(DC_DATA(m+5u))/65536.0);
        bool crushed=DC_DATA(m+4u)==0u&&compression>eq_get(profile+4u);
        if(DC_DATA(m+4u)==2u||crushed) {
            if(!crumble_member(member)) {DC_DATA(st+3u)=4u;continue;}
        } else {
            Body b=DC_BODY(slot);float length=float(b.vertices[1].x)/65536.0;
            float depth=float(DC_DATA(m+5u))/65536.0;
            bool grain=DC_DATA(m+4u)==1u;
            float work=eq_get(profile+6u)*(grain?eq_get(profile+9u)*length:depth);
            float plane=grain?depth*.5:clamp(eq_get(st+8u)*length,depth*.25,length-depth*.25);
            if(work>eq_get(st+9u)||!fragment_body(slot,grain?1u:0u,plane,DC_DATA(m+1u))) {
                DC_DATA(st+3u)=4u;continue;
            }
            DC_DATA(m+6u)|=STRUCTURAL_RETIRED;DC_DATA(st+3u)=5u;
            eq_put(st+11u,work);eq_put(st+12u,max(eq_get(st+9u)-work,0));
        }
        cut=true;
    }
    if(!cut) return;
    for(uint node=0u;node<DC_DATA(g);node++) {
        bool incident=false;uint id=DC_DATA(g+NODES+node*12u);
        for(uint m=0u;m<DC_DATA(g+1u);m++) if(member_active(m)) {
            uint p=g+MEMBERS+m*8u;
            incident=incident||DC_DATA(p+2u)==id||DC_DATA(p+3u)==id;
        }
        if(!incident) {
            DC_DATA(g+NODES+node*12u+8u)=1u;DC_DATA(state(node)+2u)=5u;
            if(DC_DATA(component_state(node))==id) DC_DATA(component_state(node)+1u)=5u;
        }
    }
    for(uint j=0u;j<DC_DATA(g+2u);j++) {
        uint p=g+JOINTS+j*16u;
        int a=node_index(g,DC_DATA(p+2u)),b=node_index(g,DC_DATA(p+3u));
        if(DC_DATA(g+NODES+uint(a)*12u+8u)!=0u||DC_DATA(g+NODES+uint(b)*12u+8u)!=0u) DC_DATA(p+14u)=1u;
    }
    DC_DATA(base()+3u)++;
}
void damage_interfaces() {
    uint g=graph();bool cut=false;
    for(uint i=0u;i<DC_DATA(g+3u)+DC_DATA(g+2u);i++) {
        bool anchor=i<DC_DATA(g+3u);uint index=anchor?i:i-DC_DATA(g+3u);
        uint p=anchor?g+ANCHORS+index*24u:g+JOINTS+index*16u;
        uint flag=p+(anchor?22u:14u),a=DC_DATA(p+2u),b=anchor?a:DC_DATA(p+3u);
        if(DC_DATA(flag)!=0u) continue;
        int node=node_index(g,a);
        if(DC_DATA(state(uint(node))+2u)>1u) continue;
        float duration=1e20;
        for(uint m=0u;m<DC_DATA(g+1u);m++) if(member_active(m)) {
            uint entry=g+MEMBERS+m*8u;
            if(DC_DATA(entry+2u)==a||DC_DATA(entry+3u)==a||DC_DATA(entry+2u)==b||DC_DATA(entry+3u)==b)
                duration=min(duration,eq_get(g+29000u+DC_DATA(entry+4u)*12u+8u));
        }
        uint slot=anchor?index:32u+index;
        float excess=max(eq_get(g+28200u+slot)-1.0,0);
        float damage=min(1.0,eq_get(g+28300u+slot)+excess*excess/(60.0*duration));
        if(damage>=1.0-1e-6) {damage=1.0;DC_DATA(flag)=1u;cut=true;}
        eq_put(g+28300u+slot,damage);
    }
    if(cut) DC_DATA(base()+3u)++;
}
void failure() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph();
    damage_members();
    damage_interfaces();
    for(uint owner=0u;owner<DC_DATA(g);owner++) {
        uint node=g+NODES+owner*12u;
        if(DC_DATA(node+8u)!=0u||DC_DATA(node)!=DC_DATA(state(owner)+1u)) continue;
        uint status=DC_DATA(state(owner)+2u),clock=failure_clock(owner);
        if(status==3u) continue;
        uint ticks=0u,members=0u,needed=0u;
        for(uint m=0u;m<DC_DATA(g+1u);m++) if(member_active(m)) {
            uint entry=g+MEMBERS+m*8u;
            int a=node_index(g,DC_DATA(entry+2u));
            if(DC_DATA(state(uint(a))+1u)!=DC_DATA(node)) continue;
            members++;
            float grace=uintBitsToFloat(DC_DATA(g+29000u+DC_DATA(entry+4u)*12u+7u));
            ticks=max(ticks,uint(ceil(grace*60.0)));
        }
        if(status<=1u) {
            for(uint n=0u;n<DC_DATA(g);n++) if(DC_DATA(state(n)+1u)==DC_DATA(node)) DC_DATA(failure_clock(n))=0u;
            continue;
        }
        if(status!=2u) continue;
        uint confirmed=0u;
        for(uint n=0u;n<DC_DATA(g);n++) if(DC_DATA(state(n)+1u)==DC_DATA(node)) confirmed=max(confirmed,DC_DATA(failure_clock(n)));
        confirmed=min(confirmed+1u,ticks);
        for(uint n=0u;n<DC_DATA(g);n++) if(DC_DATA(state(n)+1u)==DC_DATA(node)) DC_DATA(failure_clock(n))=confirmed;
        if(DC_DATA(clock)<ticks) continue;
        for(uint a=0u;a<DC_DATA(g+1u);a++) if(component_member(a,DC_DATA(node)))
            for(uint b=a+1u;b<DC_DATA(g+1u);b++) if(component_member(b,DC_DATA(node))) {
                uint shared_index;if(shared_node(a,b,shared_index)) needed++;
            }
        for(uint j=0u;j<DC_DATA(g+2u);j++) {
            uint p=g+JOINTS+j*16u;
            int a=member_at(DC_DATA(p+2u),DC_DATA(node)),b=member_at(DC_DATA(p+3u),DC_DATA(node));
            if(DC_DATA(p+14u)==0u&&a>=0&&b>=0&&a!=b) needed++;
        }
        uint free=0u;
        for(uint slot=0u;slot<DC_DATA(base()+6u);slot++) free+=uint(!bond_live(bond_record(slot)));
        bool valid=members!=0u;
        for(uint m=0u;m<DC_DATA(g+1u);m++) if(component_member(m,DC_DATA(node))) {
            uint slot=DC_DATA(g+MEMBERS+m*8u+7u);
            valid=valid&&slot<BODY_CAPACITY&&DC_BODY(slot).enabled!=0u&&
                DC_BODY(slot).reserved0==STRUCTURAL_SOURCE&&DC_BODY(slot).reserved1==m+1u;
        }
        if(!valid||needed>free) {
            for(uint n=0u;n<DC_DATA(g);n++) if(DC_DATA(state(n)+1u)==DC_DATA(node)) DC_DATA(state(n)+2u)=4u;
            DC_DATA(g+24400u+owner*16u+1u)=4u;
            continue;
        }
        /* Reservation is complete. All records and sound connections commit
         * together; no source is retired during preflight. */
        for(uint a=0u;a<DC_DATA(g+1u);a++) if(component_member(a,DC_DATA(node)))
            for(uint b=a+1u;b<DC_DATA(g+1u);b++) if(component_member(b,DC_DATA(node))) {
                uint shared_index;
                if(shared_node(a,b,shared_index)) create_bond(DC_DATA(g+MEMBERS+a*8u+7u),
                    DC_DATA(g+MEMBERS+b*8u+7u),eq_position(shared_index),2u,vec3(1e8),0,0,0);
            }
        for(uint j=0u;j<DC_DATA(g+2u);j++) {
            uint p=g+JOINTS+j*16u;
            int a=member_at(DC_DATA(p+2u),DC_DATA(node)),b=member_at(DC_DATA(p+3u),DC_DATA(node));
            if(DC_DATA(p+14u)!=0u||a<0||b<0||a==b) continue;
            vec2 point=(eq_position(uint(node_index(g,DC_DATA(p+2u))))+
                        eq_position(uint(node_index(g,DC_DATA(p+3u)))))/2.0;
            create_bond(DC_DATA(g+MEMBERS+uint(a)*8u+7u),DC_DATA(g+MEMBERS+uint(b)*8u+7u),
                        point,DC_DATA(p+4u),vec3(eq_get(p+5u),eq_get(p+6u),
                        DC_DATA(p+4u)>=2u?eq_get(p+7u):0),eq_get(p+13u),
                        eq_get(p+10u),eq_get(p+12u));
            DC_DATA(p+14u)=1u;
        }
        for(uint member=0u;member<DC_DATA(g+1u);member++) if(component_member(member,DC_DATA(node))) {
            release_member(member);
        }
        for(uint n=0u;n<DC_DATA(g);n++) if(DC_DATA(state(n)+1u)==DC_DATA(node)) {
            DC_DATA(g+NODES+n*12u+8u)=1u;DC_DATA(state(n)+2u)=5u;
        }
        DC_DATA(g+24400u+owner*16u+1u)=5u;
        DC_DATA(base()+3u)++;
    }
}
