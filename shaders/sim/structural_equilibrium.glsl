/* Test oracle is never linked here: each resident component owns disjoint GPU
 * matrix rows. The two dense workspaces are bounded by 32 nodes, not world size. */
const uint EQ_DOFS=96u,EQ_MATRIX=9216u;
shared uint eq_map[96],eq_count,eq_status;
shared float eq_rhs[96],eq_scale[96],eq_solution[96];
shared int eq_a[16],eq_b[16],eq_anchor[32];
shared vec2 eq_direction[16];
shared vec3 eq_k[16],eq_ak[32];
shared float eq_length[16],eq_weight[16];
uint eq_original() { return graph()+5024u; }
uint eq_work() { return eq_original()+EQ_MATRIX; }
float eq_get(uint offset) { return uintBitsToFloat(data[offset]); }
void eq_put(uint offset,float value) { data[offset]=floatBitsToUint(value); }
uint eq_address(uint matrix,uint row,uint col) { return matrix+row*EQ_DOFS+col; }
vec2 eq_position(uint node) {
    uint n=graph()+NODES+node*12u;
    ivec2 p;view_point(pair(n+2u),pair(n+4u),ivec2(0),p);
    return vec2(p)+vec2(int(data[n+6u]),int(data[n+7u]))/65536.0;
}
bool eq_anchor_valid(uint a) {
    uint an=graph()+ANCHORS+a*24u;
    for(uint y=0u;y<data[an+11u];y++) for(uint x=0u;x<data[an+10u];x++) {
        ivec2 p;uint c;
        if(!view_point(pair(an+4u),pair(an+6u),ivec2(data[an+8u]+x,data[an+9u]+y),p)||
           !cell_ref(p,c)||(cells[c].x&65535u)!=data[an+12u]) return false;
    }
    return true;
}
vec3 eq_vector(uint element,uint dof) {
    uint node=dof/3u,axis=dof%3u;
    bool a=int(node)==eq_a[element],b=int(node)==eq_b[element];
    if(!a&&!b) return vec3(0);
    float sign=b?1.0:-1.0,c=eq_direction[element].x,s=eq_direction[element].y;
    if(axis==0u) return vec3(sign*c,-sign*s,0);
    if(axis==1u) return vec3(sign*s,sign*c,0);
    return vec3(0,-eq_length[element]/2.0,sign);
}
float eq_entry(uint row,uint col,bool geometry) {
    uint g=graph();float k=0;
    for(uint e=0u;e<data[g+1u];e++) {
        vec3 r=eq_vector(e,row),c=eq_vector(e,col);
        vec3 stiffness=geometry?vec3(.5,1.0/(2.0+eq_length[e]*eq_length[e]/2.0),.5):eq_k[e];
        k+=dot(r*c,stiffness);
    }
    for(uint a=0u;a<data[g+3u];a++) if(eq_anchor[a]>=0&&row/3u==uint(eq_anchor[a])&&row==col) {
        float value=eq_ak[a][row%3u];k+=geometry?float(value>0):value;
    }
    /* Initial bilateral typed joint tangent: pins release relative rotation. */
    for(uint j=0u;j<data[g+2u];j++) {
        uint p=g+JOINTS+j*16u;int a=node_index(g,data[p+2u]),b=node_index(g,data[p+3u]);
        if(row%3u!=col%3u) continue;
        float rs=row/3u==uint(a)?1.0:row/3u==uint(b)?-1.0:0.0;
        float cs=col/3u==uint(a)?1.0:col/3u==uint(b)?-1.0:0.0;
        float stiffness=eq_get(p+5u+row%3u);
        if(row%3u==2u&&data[p+4u]<2u) stiffness=0;
        k+=rs*cs*(geometry?float(stiffness>0):stiffness);
    }
    return k;
}
float eq_load(uint dof) {
    float f=0;uint node=dof/3u,axis=dof%3u;
    for(uint e=0u;e<data[graph()+1u];e++) {
        if(int(node)!=eq_a[e]&&int(node)!=eq_b[e]) continue;
        float l=eq_length[e],w=eq_weight[e];
        if(axis==1u) f+=w*l/2.0;
        if(axis==2u) f+=(int(node)==eq_a[e]?1.0:-1.0)*w*eq_direction[e].x*l*l/12.0;
    }
    return f;
}
void eq_assemble(bool geometry) {
    uint lane=gl_LocalInvocationID.x;
    for(uint r=lane;r<eq_count;r+=32u) {
        uint row=eq_map[r];
        for(uint c=0u;c<eq_count;c++) {
            uint col=eq_map[c];float value=eq_entry(row,col,geometry);
            eq_put(eq_address(eq_original(),row,col),value);
        }
        float diagonal=eq_entry(row,row,geometry);
        eq_scale[r]=diagonal>0?inversesqrt(diagonal):1;
        eq_rhs[r]=geometry?0:eq_load(row);
    }
    memoryBarrierBuffer();barrier();
    for(uint r=lane;r<eq_count;r+=32u) {
        uint row=eq_map[r];eq_rhs[r]*=eq_scale[r];
        for(uint c=0u;c<eq_count;c++) {
            uint col=eq_map[c];
            eq_put(eq_address(eq_work(),row,col),
                   eq_get(eq_address(eq_original(),row,col))*eq_scale[r]*eq_scale[c]);
        }
    }
    memoryBarrierBuffer();barrier();
}
bool eq_eliminate(bool geometry) {
    uint lane=gl_LocalInvocationID.x;
    for(uint p=0u;p<eq_count;p++) {
        uint pivot=eq_map[p];float diagonal=eq_get(eq_address(eq_work(),pivot,pivot));
        if(lane==0u&&(!isnan(diagonal)&&!isinf(diagonal))) {
            if(diagonal<(geometry?1e-6:1e-7)) eq_status=geometry?2u:3u;
        }
        if(lane==0u&&(isnan(diagonal)||isinf(diagonal))) eq_status=3u;
        barrier();if(eq_status!=0u) return false;
        for(uint r=p+1u+lane;r<eq_count;r+=32u) {
            uint row=eq_map[r];float ratio=eq_get(eq_address(eq_work(),row,pivot))/diagonal;
            for(uint c=p+1u;c<eq_count;c++) {
                uint col=eq_map[c],address=eq_address(eq_work(),row,col);
                eq_put(address,eq_get(address)-ratio*eq_get(eq_address(eq_work(),pivot,col)));
            }
            eq_rhs[r]-=ratio*eq_rhs[p];
        }
        memoryBarrierBuffer();barrier();
    }
    if(!geometry&&lane==0u) for(int r=int(eq_count)-1;r>=0;r--) {
        uint row=eq_map[r];float rhs=eq_rhs[r];
        for(uint c=uint(r)+1u;c<eq_count;c++) rhs-=eq_get(eq_address(eq_work(),row,eq_map[c]))*eq_solution[c];
        eq_solution[r]=rhs/eq_get(eq_address(eq_work(),row,row));
    }
    barrier();return true;
}
void equilibrium() {
    uint g=graph(),owner=gl_WorkGroupID.x,lane=gl_LocalInvocationID.x;
    if(owner>=data[g]||data[g+NODES+owner*12u]!=data[state(owner)+1u]||
       data[state(owner)+4u]!=0u||data[state(owner)+3u]==0u) return;
    if(lane==0u) {
        eq_count=0u;eq_status=0u;
        for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[state(owner)+1u])
            for(uint d=0u;d<3u;d++) eq_map[eq_count++]=n*3u+d;
    }
    for(uint e=lane;e<data[g+1u];e+=32u) {
        uint m=g+MEMBERS+e*8u;eq_a[e]=node_index(g,data[m+2u]);eq_b[e]=node_index(g,data[m+3u]);
        vec2 delta=eq_position(uint(eq_b[e]))-eq_position(uint(eq_a[e]));
        float l=length(delta),depth=float(data[m+5u])/65536.0;
        eq_length[e]=l;eq_direction[e]=delta/l;
        uint profile=g+29000u+data[m+4u]*12u;
        float young=eq_get(profile+1u),shear=eq_get(profile+2u),inertia=depth*depth*depth/12.0;
        float ei=young*inertia,phi=12.0*ei/((5.0/6.0)*shear*depth*l*l);
        eq_k[e]=vec3(young*depth/l,12.0*ei/((1.0+phi)*l*l*l),ei/l);
        eq_weight[e]=eq_get(profile)*depth*900.0;
    }
    for(uint a=lane;a<data[g+3u];a+=32u) {
        uint an=g+ANCHORS+a*24u;eq_anchor[a]=eq_anchor_valid(a)?node_index(g,data[an+2u]):-1;
        eq_ak[a]=vec3(eq_get(an+13u),eq_get(an+14u),data[an+3u]>=2u?eq_get(an+15u):0);
    }
    barrier();eq_assemble(true);
    bool ranked=eq_eliminate(true);
    if(ranked) {eq_assemble(false);eq_eliminate(false);}
    if(eq_status==0u) {
        for(uint r=lane;r<eq_count;r+=32u) eq_solution[r]*=eq_scale[r];
        barrier();
        for(uint r=lane;r<eq_count;r+=32u) {
            uint row=eq_map[r];float sum=0,scale=abs(eq_load(row));
            for(uint c=0u;c<eq_count;c++) {
                float term=eq_get(eq_address(eq_original(),row,eq_map[c]))*eq_solution[c];
                sum+=term;scale+=abs(term);
            }
            float residual=abs(sum-eq_load(row))/max(scale,1.0);
            if(isnan(residual)||isinf(residual)||residual>1e-4) atomicMax(eq_status,3u);
        }
        barrier();
    }
    for(uint r=lane;r<eq_count;r+=32u) {
        uint dof=eq_map[r],st=state(dof/3u),axis=dof%3u;
        if(axis==0u) data[st+2u]=eq_status;
        float q=eq_status==0u?eq_solution[r]:0;
        eq_put(st+6u+axis,q);float reaction=0;
        for(uint a=0u;a<data[g+3u];a++) if(eq_anchor[a]==int(dof/3u)) reaction-=eq_ak[a][axis]*q;
        eq_put(st+9u+axis,reaction);
    }
}
