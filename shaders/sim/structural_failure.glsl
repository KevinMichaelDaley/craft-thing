/* Timers use integer fixed physics ticks, never wall time or fluid step duration.
 * Sound members reuse their reserved collision record without a second copy. */
const uint STRUCTURAL_RETIRED=2u;
#include "structural_bonds.glsl"
uint failure_clock(uint node) { return graph()+28000u+node*4u; }
bool member_active(uint member) {
    return (data[graph()+MEMBERS+member*8u+6u]&STRUCTURAL_RETIRED)==0u;
}
bool component_member(uint member,uint id) {
    uint m=graph()+MEMBERS+member*8u;
    int a=node_index(graph(),data[m+2u]);
    return member_active(member)&&data[state(uint(a))+1u]==id;
}
bool shared_node(uint a,uint b,out uint node) {
    uint g=graph(),ma=g+MEMBERS+a*8u,mb=g+MEMBERS+b*8u;
    for(uint x=2u;x<4u;x++) for(uint y=2u;y<4u;y++)
        if(data[ma+x]==data[mb+y]) {node=uint(node_index(g,data[ma+x]));return true;}
    return false;
}
int member_at(uint node,uint component) {
    for(uint m=0u;m<data[graph()+1u];m++) if(component_member(m,component)) {
        uint p=graph()+MEMBERS+m*8u;
        if(data[p+2u]==node||data[p+3u]==node) return int(m);
    }
    return -1;
}
void create_bond(uint a,uint b,vec2 point,uint kind,vec3 stiffness,float normal) {
    uint slot=0u;
    while(slot<STRUCTURAL_BOND_CAPACITY&&bond_live(bond_record(slot))) slot++;
    uint p=bond_record(slot);Body ba=bodies[a],bb=bodies[b];
    for(uint word=0u;word<STRUCTURAL_BOND_WORDS;word++) data[p+word]=0u;
    data[p]=1u;data[p+1u]=a;data[p+2u]=b;data[p+3u]=ba.id;data[p+4u]=bb.id;data[p+5u]=kind;
    vec2 pa=body_rotate(point-vec2(ba.x_fp,ba.y_fp)/65536.0-ba.center,-ba.angle);
    vec2 pb=body_rotate(point-vec2(bb.x_fp,bb.y_fp)/65536.0-bb.center,-bb.angle);
    for(uint axis=0u;axis<2u;axis++) {
        data[p+6u+axis]=floatBitsToUint(pa[axis]);data[p+8u+axis]=floatBitsToUint(pb[axis]);
    }
    data[p+10u]=floatBitsToUint(bb.angle-ba.angle);
    data[p+11u]=floatBitsToUint(normal-ba.angle);
    for(uint axis=0u;axis<3u;axis++) data[p+12u+axis]=floatBitsToUint(stiffness[axis]);
}
void failure() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph();
    for(uint owner=0u;owner<data[g];owner++) {
        uint node=g+NODES+owner*12u;
        if(data[node+8u]!=0u||data[node]!=data[state(owner)+1u]) continue;
        uint status=data[state(owner)+2u],clock=failure_clock(owner);
        if(status==3u) continue;
        uint ticks=0u,members=0u,needed=0u;
        for(uint m=0u;m<data[g+1u];m++) if(member_active(m)) {
            uint entry=g+MEMBERS+m*8u;
            int a=node_index(g,data[entry+2u]);
            if(data[state(uint(a))+1u]!=data[node]) continue;
            members++;
            float grace=uintBitsToFloat(data[g+29000u+data[entry+4u]*12u+7u]);
            ticks=max(ticks,uint(ceil(grace*60.0)));
        }
        if(status==0u) {data[clock]=0u;continue;}
        if(status!=2u) continue;
        data[clock]=min(data[clock]+1u,ticks);
        if(data[clock]<ticks) continue;
        for(uint a=0u;a<data[g+1u];a++) if(component_member(a,data[node]))
            for(uint b=a+1u;b<data[g+1u];b++) if(component_member(b,data[node])) {
                uint shared_index;if(shared_node(a,b,shared_index)) needed++;
            }
        for(uint j=0u;j<data[g+2u];j++) {
            uint p=g+JOINTS+j*16u;
            int a=member_at(data[p+2u],data[node]),b=member_at(data[p+3u],data[node]);
            if(data[p+14u]==0u&&a>=0&&b>=0&&a!=b) needed++;
        }
        uint free=0u;
        for(uint slot=0u;slot<STRUCTURAL_BOND_CAPACITY;slot++) free+=uint(!bond_live(bond_record(slot)));
        bool valid=members!=0u;
        for(uint m=0u;m<data[g+1u];m++) if(component_member(m,data[node])) {
            uint slot=data[g+MEMBERS+m*8u+7u];
            valid=valid&&slot<BODY_CAPACITY&&bodies[slot].enabled!=0u&&
                bodies[slot].reserved0==STRUCTURAL_SOURCE&&bodies[slot].reserved1==m+1u;
        }
        if(!valid||needed>free) {
            for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[node]) data[state(n)+2u]=4u;
            data[g+24400u+owner*16u+1u]=4u;
            continue;
        }
        /* Reservation is complete. All records and sound connections commit
         * together; no source is retired during preflight. */
        for(uint a=0u;a<data[g+1u];a++) if(component_member(a,data[node]))
            for(uint b=a+1u;b<data[g+1u];b++) if(component_member(b,data[node])) {
                uint shared_index;
                if(shared_node(a,b,shared_index)) create_bond(data[g+MEMBERS+a*8u+7u],
                    data[g+MEMBERS+b*8u+7u],eq_position(shared_index),2u,vec3(1e8),0);
            }
        for(uint j=0u;j<data[g+2u];j++) {
            uint p=g+JOINTS+j*16u;
            int a=member_at(data[p+2u],data[node]),b=member_at(data[p+3u],data[node]);
            if(data[p+14u]!=0u||a<0||b<0||a==b) continue;
            vec2 point=(eq_position(uint(node_index(g,data[p+2u])))+
                        eq_position(uint(node_index(g,data[p+3u]))))/2.0;
            create_bond(data[g+MEMBERS+uint(a)*8u+7u],data[g+MEMBERS+uint(b)*8u+7u],
                        point,data[p+4u],vec3(eq_get(p+5u),eq_get(p+6u),
                        data[p+4u]>=2u?eq_get(p+7u):0),eq_get(p+13u));
            data[p+14u]=1u;
        }
        for(uint member=0u;member<data[g+1u];member++) if(component_member(member,data[node])) {
            uint m=g+MEMBERS+member*8u,slot=data[m+7u];Body b=bodies[slot];
            b.motion_flags=0u;b.motion_ready=0u;b.reserved0=0u;b.reserved1=0u;
            b.vx_fp=0;b.vy_fp=0;b.angular_velocity=0;
            b.rest_ticks=0u;body_properties(b);bodies[slot]=b;
            data[m+6u]|=STRUCTURAL_RETIRED;
            data[g+24000u+member*16u+3u]=5u;
        }
        for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[node]) {
            data[g+NODES+n*12u+8u]=1u;data[state(n)+2u]=5u;
        }
        data[g+24400u+owner*16u+1u]=5u;
        data[base()+3u]++;
    }
}
