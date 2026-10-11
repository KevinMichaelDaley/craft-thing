shared vec3 eq_member_force[16],eq_anchor_force[32],eq_joint_force[64];
shared float eq_delta[96];
void eq_physical_prepare() {
    uint g=graph(),lane=gl_LocalInvocationID.x;
    for(uint e=lane;e<data[g+1u];e+=32u) {
        vec3 strain=vec3(0);
        for(uint axis=0u;axis<3u;axis++) {
            uint a=uint(eq_a[e])*3u+axis,b=uint(eq_b[e])*3u+axis;
            strain+=eq_vector(e,a)*eq_q(a)+eq_vector(e,b)*eq_q(b);
        }
        eq_member_force[e]=eq_k[e]*strain;
    }
    for(uint a=lane;a<data[g+3u];a+=32u) eq_anchor_force[a]=eq_ak[a]*anchor_gap(a);
    for(uint j=lane;j<data[g+2u];j+=32u) eq_joint_force[j]=eq_joint_k[j]*joint_gap(j);
    barrier();
}
vec2 eq_physical(uint dof) {
    float sum=0,scale=0;uint g=graph();
    for(uint e=0u;e<data[g+1u];e++) {
        float force=dot(eq_vector(e,dof),eq_member_force[e]);sum+=force;scale+=abs(force);
    }
    for(uint a=0u;a<data[g+3u];a++) {
        float force=dot(anchor_vector(a,dof),eq_anchor_force[a]);sum+=force;scale+=abs(force);
    }
    for(uint j=0u;j<data[g+2u];j++) {
        float force=dot(joint_vector(j,dof),eq_joint_force[j]);sum+=force;scale+=abs(force);
    }
    return vec2(sum,scale);
}
void eq_refine() {
    uint lane=gl_LocalInvocationID.x;
    for(uint iteration=0u;iteration<3u;iteration++) {
        eq_physical_prepare();
        for(uint r=lane;r<eq_count;r+=32u)
            eq_rhs[r]=(eq_load(eq_map[r])-eq_physical(eq_map[r]).x)*eq_scale[r];
        barrier();
        for(uint p=0u;p<eq_count;p++) {
            for(uint r=p+1u+lane;r<eq_count;r+=32u)
                eq_rhs[r]-=eq_get(eq_address(eq_work(),eq_map[r],eq_map[p]))*eq_rhs[p];
            barrier();
        }
        if(lane==0u) for(int r=int(eq_count)-1;r>=0;r--) {
            uint row=eq_map[r];float rhs=eq_rhs[r];
            for(uint c=uint(r)+1u;c<eq_count;c++) rhs-=eq_get(eq_address(eq_work(),row,eq_map[c]))*eq_delta[c];
            eq_delta[r]=rhs/eq_get(eq_address(eq_work(),row,row));
        }
        barrier();
        for(uint r=lane;r<eq_count;r+=32u) eq_solution[r]+=eq_delta[r]*eq_scale[r];
        barrier();
    }
    eq_physical_prepare();
}
