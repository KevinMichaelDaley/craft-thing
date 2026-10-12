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
void create_bond(uint a,uint b,vec2 point,uint kind,vec3 stiffness,float normal,
                 float cohesion,float friction) {
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
    data[p+18u]=floatBitsToUint(cohesion);data[p+19u]=floatBitsToUint(friction);
    data[base()+9u]=1u;data[base()+10u]=1u;data[base()+11u]=1u;
}
void release_member(uint member) {
    uint g=graph(),m=g+MEMBERS+member*8u,slot=data[m+7u];
    Body b=bodies[slot];
    b.motion_flags=0u;b.motion_ready=0u;b.reserved0=0x53545232u;b.reserved1=data[m+1u];
    b.vx_fp=0;b.vy_fp=0;b.angular_velocity=0;
    b.rest_ticks=0u;body_properties(b);bodies[slot]=b;
    data[m+6u]|=STRUCTURAL_RETIRED;
    data[g+24000u+member*16u+3u]=5u;
}
#include "structural_debris.glsl"
#include "rigid_fragment.glsl"
void damage_members() {
    uint g=graph();bool cut=false;
    for(uint member=0u;member<data[g+1u];member++) if(member_active(member)) {
        uint m=g+MEMBERS+member*8u,st=member_state(member);
        if(data[st+3u]>=2u) continue;
        float excess=max(eq_get(st+7u)-1.0,0);
        uint profile=g+29000u+data[m+4u]*12u;
        float damage=min(1.0,eq_get(st+10u)+excess*excess/(60.0*eq_get(profile+8u)));
        eq_put(st+10u,damage);
        if(damage<1.0) continue;
        uint slot=data[m+7u];
        if(slot>=BODY_CAPACITY||bodies[slot].reserved0!=STRUCTURAL_SOURCE||
           bodies[slot].reserved1!=member+1u||bodies[slot].enabled==0u) {
            data[st+3u]=4u;continue;
        }
        if(data[m+4u]==2u) {
            if(!crumble_member(member)) {data[st+3u]=4u;continue;}
        } else if(data[m+4u]==0u) {
            Body b=bodies[slot];float length=float(b.vertices[1].x)/65536.0;
            float depth=float(data[m+5u])/65536.0,work=eq_get(profile+6u)*depth;
            if(work>eq_get(st+9u)||!fragment_body(slot,0u,
                clamp(eq_get(st+8u)*length,depth*.25,length-depth*.25),data[m+1u])) {
                data[st+3u]=4u;continue;
            }
            data[m+6u]|=STRUCTURAL_RETIRED;data[st+3u]=5u;
            eq_put(st+11u,work);eq_put(st+12u,max(eq_get(st+9u)-work,0));
        } else release_member(member);
        cut=true;
    }
    if(!cut) return;
    for(uint node=0u;node<data[g];node++) {
        bool incident=false;uint id=data[g+NODES+node*12u];
        for(uint m=0u;m<data[g+1u];m++) if(member_active(m)) {
            uint p=g+MEMBERS+m*8u;
            incident=incident||data[p+2u]==id||data[p+3u]==id;
        }
        if(!incident) {
            data[g+NODES+node*12u+8u]=1u;data[state(node)+2u]=5u;
            if(data[component_state(node)]==id) data[component_state(node)+1u]=5u;
        }
    }
    for(uint j=0u;j<data[g+2u];j++) {
        uint p=g+JOINTS+j*16u;
        int a=node_index(g,data[p+2u]),b=node_index(g,data[p+3u]);
        if(data[g+NODES+uint(a)*12u+8u]!=0u||data[g+NODES+uint(b)*12u+8u]!=0u) data[p+14u]=1u;
    }
    data[base()+3u]++;
}
void damage_interfaces() {
    uint g=graph();bool cut=false;
    for(uint i=0u;i<data[g+3u]+data[g+2u];i++) {
        bool anchor=i<data[g+3u];uint index=anchor?i:i-data[g+3u];
        uint p=anchor?g+ANCHORS+index*24u:g+JOINTS+index*16u;
        uint flag=p+(anchor?22u:14u),a=data[p+2u],b=anchor?a:data[p+3u];
        if(data[flag]!=0u) continue;
        int node=node_index(g,a);
        if(data[state(uint(node))+2u]>1u) continue;
        float duration=1e20;
        for(uint m=0u;m<data[g+1u];m++) if(member_active(m)) {
            uint entry=g+MEMBERS+m*8u;
            if(data[entry+2u]==a||data[entry+3u]==a||data[entry+2u]==b||data[entry+3u]==b)
                duration=min(duration,eq_get(g+29000u+data[entry+4u]*12u+8u));
        }
        uint slot=anchor?index:32u+index;
        float excess=max(eq_get(g+28200u+slot)-1.0,0);
        float damage=min(1.0,eq_get(g+28300u+slot)+excess*excess/(60.0*duration));
        if(damage>=1.0-1e-6) {damage=1.0;data[flag]=1u;cut=true;}
        eq_put(g+28300u+slot,damage);
    }
    if(cut) data[base()+3u]++;
}
void failure() {
    if(gl_LocalInvocationID.x!=0u) return;
    uint g=graph();
    damage_members();
    damage_interfaces();
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
        if(status<=1u) {data[clock]=0u;continue;}
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
        for(uint slot=0u;slot<data[base()+6u];slot++) free+=uint(!bond_live(bond_record(slot)));
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
                    data[g+MEMBERS+b*8u+7u],eq_position(shared_index),2u,vec3(1e8),0,0,0);
            }
        for(uint j=0u;j<data[g+2u];j++) {
            uint p=g+JOINTS+j*16u;
            int a=member_at(data[p+2u],data[node]),b=member_at(data[p+3u],data[node]);
            if(data[p+14u]!=0u||a<0||b<0||a==b) continue;
            vec2 point=(eq_position(uint(node_index(g,data[p+2u])))+
                        eq_position(uint(node_index(g,data[p+3u]))))/2.0;
            create_bond(data[g+MEMBERS+uint(a)*8u+7u],data[g+MEMBERS+uint(b)*8u+7u],
                        point,data[p+4u],vec3(eq_get(p+5u),eq_get(p+6u),
                        data[p+4u]>=2u?eq_get(p+7u):0),eq_get(p+13u),
                        eq_get(p+10u),eq_get(p+12u));
            data[p+14u]=1u;
        }
        for(uint member=0u;member<data[g+1u];member++) if(component_member(member,data[node])) {
            release_member(member);
        }
        for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[node]) {
            data[g+NODES+n*12u+8u]=1u;data[state(n)+2u]=5u;
        }
        data[g+24400u+owner*16u+1u]=5u;
        data[base()+3u]++;
    }
}
