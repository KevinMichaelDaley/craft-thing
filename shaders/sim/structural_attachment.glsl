shared vec2 eq_normal[32],eq_joint_normal[64];
shared ivec2 eq_joint_nodes[64];
shared vec3 eq_joint_k[64],eq_joint_v[64][6];
shared uint eq_changed,eq_iterations;
float eq_q(uint dof);
vec3 anchor_vector(uint a,uint dof) {
    if(eq_anchor[a]<0||dof/3u!=uint(eq_anchor[a])) return vec3(0);
    vec2 n=eq_normal[a];
    return dof%3u==0u?vec3(n.x,-n.y,0):dof%3u==1u?vec3(n.y,n.x,0):vec3(0,0,1);
}
vec3 joint_vector(uint j,uint dof) {
    uint node=dof/3u,axis=dof%3u;
    if(int(node)==eq_joint_nodes[j].x) return eq_joint_v[j][axis];
    if(int(node)==eq_joint_nodes[j].y) return eq_joint_v[j][axis+3u];
    return vec3(0);
}
vec3 joint_geometry(uint j) {
    vec3 square=vec3(0);
    for(uint d=0u;d<6u;d++) square+=eq_joint_v[j][d]*eq_joint_v[j][d];
    return vec3(eq_joint_k[j].x>0?1.0/square.x:0,
        eq_joint_k[j].y>0?1.0/square.y:0,eq_joint_k[j].z>0?1.0/square.z:0);
}
vec3 anchor_gap(uint a) {
    vec3 gap=vec3(0);
    if(eq_anchor[a]<0) return gap;
    for(uint d=0u;d<3u;d++) gap+=anchor_vector(a,uint(eq_anchor[a])*3u+d)*eq_q(uint(eq_anchor[a])*3u+d);
    return gap;
}
vec3 joint_gap(uint j) {
    vec3 gap=vec3(0);
    for(uint d=0u;d<3u;d++) {
        gap+=eq_joint_v[j][d]*eq_q(uint(eq_joint_nodes[j].x)*3u+d);
        gap+=eq_joint_v[j][d+3u]*eq_q(uint(eq_joint_nodes[j].y)*3u+d);
    }
    return gap;
}
float anchor_reaction(uint a,uint dof) {
    return -dot(anchor_vector(a,dof),eq_ak[a]*anchor_gap(a));
}
void update_bearings(uint owner) {
    uint g=graph(),lane=gl_LocalInvocationID.x;
    if(lane==0u) eq_changed=0u;
    barrier();
    for(uint a=lane;a<data[g+3u];a+=32u) if(eq_anchor[a]>=0&&
        data[state(uint(eq_anchor[a]))+1u]==data[state(owner)+1u]) {
        uint an=g+ANCHORS+a*24u;
        if(data[an+3u]!=0u) continue;
        float gap=anchor_gap(a).x;
        bool open=gap>1e-7,closed=gap<-1e-7;
        vec3 original=vec3(eq_get(an+13u),eq_get(an+14u),0);
        if(eq_get(an+18u)==0&&eq_get(an+20u)==0) original.y=0;
        vec3 next=open?vec3(0):closed?original:eq_ak[a];
        if(any(notEqual(next,eq_ak[a]))) {eq_ak[a]=next;atomicOr(eq_changed,1u);}
    }
    for(uint j=lane;j<data[g+2u];j+=32u) if(
        data[state(uint(eq_joint_nodes[j].x))+1u]==data[state(owner)+1u]) {
        uint p=g+JOINTS+j*16u;
        if(data[p+4u]!=0u) continue;
        float gap=joint_gap(j).x;
        bool open=gap>1e-7,closed=gap<-1e-7;
        vec3 original=vec3(eq_get(p+5u),eq_get(p+6u),0);
        if(eq_get(p+10u)==0&&eq_get(p+12u)==0) original.y=0;
        vec3 next=open?vec3(0):closed?original:eq_joint_k[j];
        if(any(notEqual(next,eq_joint_k[j]))) {eq_joint_k[j]=next;atomicOr(eq_changed,1u);}
    }
    barrier();
}
