/* Test oracle is never linked here: each resident component owns disjoint GPU
 * matrix rows. The two dense workspaces are bounded by 32 nodes, not world size. */
const uint EQ_DOFS=96u,EQ_MATRIX=9216u;
shared uint eq_map[96],eq_count,eq_status;
shared float eq_rhs[96],eq_scale[96],eq_solution[96];
shared float eq_minimum;
shared float eq_geometry_minimum;
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
#include "structural_attachment.glsl"
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
    for(uint a=0u;a<data[g+3u];a++) {
        vec3 stiffness=geometry?vec3(greaterThan(eq_ak[a],vec3(0))):eq_ak[a];
        vec3 r=anchor_vector(a,row),c=anchor_vector(a,col);
        k+=dot(r*c,stiffness);
        if(!geometry) k+=r.y*eq_anchor_slide[a].x*eq_ak[a].x*c.x;
    }
    for(uint j=0u;j<data[g+2u];j++) {
        vec3 r=joint_vector(j,row),c=joint_vector(j,col);
        k+=dot(r*c,geometry?joint_geometry(j):eq_joint_k[j]);
        if(!geometry) k+=r.y*eq_joint_slide[j].x*eq_joint_k[j].x*c.x;
    }
    return k;
}
float eq_load(uint dof) {
    float f=float(int(data[structural_loads()+dof]))*3600.0/STRUCTURAL_LOAD_SCALE;
    uint node=dof/3u,axis=dof%3u;
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
        float bias=0;
        for(uint a=0u;a<data[graph()+3u];a++) bias+=anchor_vector(a,row).y*eq_anchor_slide[a].y;
        for(uint j=0u;j<data[graph()+2u];j++) bias+=joint_vector(j,row).y*eq_joint_slide[j].y;
        eq_rhs[r]=geometry?0:eq_load(row)-bias;
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
#include "structural_rank.glsl"
bool eq_eliminate(bool geometry) {
    uint lane=gl_LocalInvocationID.x;
    for(uint p=0u;p<eq_count;p++) {
        uint pivot=eq_map[p];float diagonal=eq_get(eq_address(eq_work(),pivot,pivot));
        if(lane==0u) {
            eq_iterations++;
            if(geometry) eq_geometry_minimum=min(eq_geometry_minimum,diagonal);
            else eq_minimum=min(eq_minimum,diagonal);
        }
        if(lane==0u&&(!isnan(diagonal)&&!isinf(diagonal))) {
            if(diagonal<(geometry?1e-6:1e-7))
                eq_status=geometry&&eq_mechanism_proven(gl_WorkGroupID.x)?2u:3u;
        }
        if(lane==0u&&(isnan(diagonal)||isinf(diagonal))) eq_status=3u;
        barrier();if(eq_status!=0u) return false;
        for(uint r=p+1u+lane;r<eq_count;r+=32u) {
            uint row=eq_map[r];float ratio=eq_get(eq_address(eq_work(),row,pivot))/diagonal;
            for(uint c=p+1u;c<eq_count;c++) {
                uint col=eq_map[c],address=eq_address(eq_work(),row,col);
                eq_put(address,eq_get(address)-ratio*eq_get(eq_address(eq_work(),pivot,col)));
            }
            eq_put(eq_address(eq_work(),row,pivot),ratio);
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
vec2 eq_physical(uint dof);
#include "structural_stress.glsl"
#include "structural_refine.glsl"
void equilibrium() {
    uint g=graph(),owner=gl_WorkGroupID.x,lane=gl_LocalInvocationID.x;
    if(owner>=data[g]||data[g+NODES+owner*12u]!=data[state(owner)+1u]||
       data[state(owner)+4u]!=0u||data[state(owner)+3u]==0u) return;
    uint component_mask=0u,invalid=0u;
    for(uint n=0u;n<data[g];n++) if(data[state(n)+1u]==data[state(owner)+1u]) {
        component_mask|=1u<<n;if(data[structural_bad()+n]!=0u) invalid|=2u;
    }
    for(uint b=0u;b<params.body_count;b++)
        if((data[structural_dependencies()+b]&component_mask)!=0u&&data[structural_clipped()+b]!=0u) invalid|=4u;
    if(data[base()-16u-64u*32u-8192u*16u+3u]!=0u) invalid|=2u;
    if(invalid!=0u) {if(lane==0u) data[component_state(owner)+3u]=invalid;return;}
    if(lane==0u) {
        eq_count=0u;eq_status=0u;eq_minimum=1;eq_geometry_minimum=1;
        eq_iterations=0u;eq_active_iterations=0u;eq_refinement_iterations=0u;
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
        eq_anchor_slide[a]=vec2(0);
        if(data[an+3u]==0u&&eq_get(an+18u)==0&&eq_get(an+20u)==0) eq_ak[a].y=0;
        float angle=eq_get(an+21u);eq_normal[a]=vec2(cos(angle),sin(angle));
    }
    for(uint j=lane;j<data[g+2u];j+=32u) {
        uint p=g+JOINTS+j*16u;int a=node_index(g,data[p+2u]),b=node_index(g,data[p+3u]);
        eq_joint_nodes[j]=ivec2(a,b);
        float angle=eq_get(p+13u);vec2 n=vec2(cos(angle),sin(angle)),t=vec2(-n.y,n.x);
        vec2 arm=(eq_position(uint(b))-eq_position(uint(a)))/2.0;
        eq_joint_normal[j]=n;
        eq_joint_v[j][0]=vec3(n.x,t.x,0);eq_joint_v[j][1]=vec3(n.y,t.y,0);
        eq_joint_v[j][2]=vec3(body_cross(arm,n),body_cross(arm,t),1);
        eq_joint_v[j][3]=vec3(-n.x,-t.x,0);eq_joint_v[j][4]=vec3(-n.y,-t.y,0);
        eq_joint_v[j][5]=vec3(body_cross(arm,n),body_cross(arm,t),-1);
        eq_joint_k[j]=vec3(eq_get(p+5u),eq_get(p+6u),data[p+4u]>=2u?eq_get(p+7u):0);
        eq_joint_slide[j]=vec2(0);
        if(data[p+4u]==0u&&eq_get(p+10u)==0&&eq_get(p+12u)==0) eq_joint_k[j].y=0;
    }
    barrier();
    for(uint iteration=0u;iteration<8u;iteration++) {
        if(lane==0u) eq_active_iterations++;
        eq_assemble(true);bool ranked=eq_eliminate(true);
        if(ranked) {eq_assemble(false);eq_eliminate(false);}
        if(eq_status!=0u) break;
        for(uint r=lane;r<eq_count;r+=32u) eq_solution[r]*=eq_scale[r];
        barrier();eq_refine();update_bearings(owner);
        if(eq_changed==0u) break;
        if(iteration==7u&&lane==0u) eq_status=3u;
        barrier();
    }
    if(eq_status==0u) {
        for(uint r=lane;r<eq_count;r+=32u) {
            uint row=eq_map[r];vec2 physical=eq_physical(row);
            float residual=abs(physical.x-eq_load(row))/max(physical.y+abs(eq_load(row)),1.0);
            if(isnan(residual)||isinf(residual)||residual>1e-4) atomicMax(eq_status,3u);
        }
        barrier();
    }
    bool solved=eq_status==0u;
    barrier();
    if(solved) {
        for(uint e=lane;e<data[g+1u];e+=32u)
            if(data[state(uint(eq_a[e]))+1u]==data[state(owner)+1u]) member_stress(e);
        for(uint a=lane;a<data[g+3u];a+=32u) if(eq_anchor[a]>=0&&
            data[state(uint(eq_anchor[a]))+1u]==data[state(owner)+1u]) {
            vec3 demand=anchor_demand(a);
            if(attachment_ratio(demand,g+ANCHORS+a*24u+16u)>1) atomicMax(eq_status,1u);
        }
        for(uint j=lane;j<data[g+2u];j+=32u) if(data[state(uint(eq_joint_nodes[j].x))+1u]==data[state(owner)+1u])
            if(attachment_ratio(joint_demand(j),g+JOINTS+j*16u+8u)>1.00001) atomicMax(eq_status,1u);
    }
    barrier();
    if(lane==0u&&eq_status<=1u) component_budget(owner);
    if(lane==0u&&eq_status>=2u) {
        uint diagnostic=component_state(owner);
        data[diagnostic+1u]=eq_status;data[diagnostic+3u]=eq_status==2u?16u:8u;
        eq_put(diagnostic+11u,eq_minimum);data[diagnostic+12u]=eq_iterations;
        data[diagnostic+13u]=eq_active_iterations;data[diagnostic+14u]=eq_refinement_iterations;
        eq_put(diagnostic+15u,eq_geometry_minimum);
    }
    for(uint e=lane;e<data[g+1u];e+=32u) if(eq_status>=2u&&
        data[state(uint(eq_a[e]))+1u]==data[state(owner)+1u]) data[member_state(e)+3u]=eq_status;
    for(uint r=lane;r<eq_count;r+=32u) {
        uint dof=eq_map[r],st=state(dof/3u),axis=dof%3u;
        if(axis==0u) data[st+2u]=eq_status;
        float q=eq_status<=1u?eq_solution[r]:0;
        eq_put(st+6u+axis,q);float reaction=0;
        if(eq_status<=1u) for(uint a=0u;a<data[g+3u];a++) reaction+=anchor_reaction(a,dof);
        eq_put(st+9u+axis,reaction);
    }
}
