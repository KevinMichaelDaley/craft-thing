shared vec2 eq_normal[32],eq_joint_normal[64];
shared ivec2 eq_joint_nodes[64];
shared vec3 eq_joint_k[64],eq_joint_v[64][6];
shared vec2 eq_anchor_slide[32],eq_joint_slide[64];
shared uint eq_changed,eq_iterations;
shared uint eq_active_iterations,eq_refinement_iterations;
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
vec3 anchor_demand(uint a) {
    vec3 demand=eq_ak[a]*anchor_gap(a);
    demand.y+=eq_anchor_slide[a].x*demand.x+eq_anchor_slide[a].y;
    return demand;
}
vec3 joint_demand(uint j) {
    vec3 demand=eq_joint_k[j]*joint_gap(j);
    demand.y+=eq_joint_slide[j].x*demand.x+eq_joint_slide[j].y;
    return demand;
}
float anchor_reaction(uint a,uint dof) {
    return -dot(anchor_vector(a,dof),anchor_demand(a));
}
void update_bearings(uint owner) {
    uint g=graph(),lane=gl_LocalInvocationID.x;
    if(lane==0u) eq_changed=0u;
    barrier();
    for(uint a=lane;a<DC_DATA(g+3u);a+=32u) if(eq_anchor[a]>=0&&
        DC_DATA(state(uint(eq_anchor[a]))+1u)==DC_DATA(state(owner)+1u)) {
        uint an=g+ANCHORS+a*24u;
        if(DC_DATA(an+3u)!=0u) continue;
        vec3 gap=anchor_gap(a);
        bool open=gap.x>1e-9,closed=gap.x<-1e-9;
        vec3 original=vec3(eq_get(an+13u),eq_get(an+14u),0);
        if(eq_get(an+18u)==0&&eq_get(an+20u)==0) original.y=0;
        vec3 next=open?vec3(0):closed?original:eq_ak[a];
        vec2 slide=vec2(0);
        if(!open) {
            next=original;float normal=next.x*gap.x,trial=next.y*gap.y;
            float cohesion=eq_get(an+18u),mu=eq_get(an+20u);
            float limit=cohesion+mu*max(-normal,0);
            if(limit==0) next.y=0;
            else if(abs(trial)>limit) {
                next.y=0;slide=vec2(normal<0?-sign(trial)*mu:0,sign(trial)*cohesion);
            }
        }
        if(any(notEqual(next,eq_ak[a]))||any(notEqual(slide,eq_anchor_slide[a]))) {
            eq_ak[a]=next;eq_anchor_slide[a]=slide;atomicOr(eq_changed,1u);
        }
    }
    for(uint j=lane;j<DC_DATA(g+2u);j+=32u) if(
        DC_DATA(state(uint(eq_joint_nodes[j].x))+1u)==DC_DATA(state(owner)+1u)) {
        uint p=g+JOINTS+j*16u;
        if(DC_DATA(p+4u)!=0u||DC_DATA(p+14u)!=0u) continue;
        vec3 gap=joint_gap(j);
        bool open=gap.x>1e-9,closed=gap.x<-1e-9;
        vec3 original=vec3(eq_get(p+5u),eq_get(p+6u),0);
        if(eq_get(p+10u)==0&&eq_get(p+12u)==0) original.y=0;
        vec3 next=open?vec3(0):closed?original:eq_joint_k[j];
        vec2 slide=vec2(0);
        if(!open) {
            next=original;float normal=next.x*gap.x,trial=next.y*gap.y;
            float cohesion=eq_get(p+10u),mu=eq_get(p+12u);
            float limit=cohesion+mu*max(-normal,0);
            if(limit==0) next.y=0;
            else if(abs(trial)>limit) {
                next.y=0;slide=vec2(normal<0?-sign(trial)*mu:0,sign(trial)*cohesion);
            }
        }
        if(any(notEqual(next,eq_joint_k[j]))||any(notEqual(slide,eq_joint_slide[j]))) {
            eq_joint_k[j]=next;eq_joint_slide[j]=slide;atomicOr(eq_changed,1u);
        }
    }
    barrier();
}
