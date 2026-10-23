uint member_state(uint member) { return graph()+24000u+member*16u; }
uint component_state(uint owner) { return graph()+24400u+owner*16u; }
float eq_q(uint dof) {
    for(uint r=0u;r<eq_count;r++) if(eq_map[r]==dof) return eq_solution[r];
    return 0;
}
float capacity_ratio(float demand,float capacity) {
    return demand==0?0:capacity>0?abs(demand)/capacity:1e20;
}
float attachment_ratio(vec3 demand,uint p) {
    float u=capacity_ratio(demand.x,eq_get(p+(demand.x>0?1u:0u)));
    u=max(u,capacity_ratio(demand.y,eq_get(p+2u)+eq_get(p+4u)*max(-demand.x,0)));
    return max(u,capacity_ratio(demand.z,eq_get(p+3u)));
}
vec3 member_point(uint e,uint bin) {
    uint p=structural_bins()+(e*17u+bin)*3u;
    return vec3(int(DC_DATA(p)),int(DC_DATA(p+1u)),int(DC_DATA(p+2u)))*3600.0/STRUCTURAL_LOAD_SCALE;
}
vec3 member_resultant(float x,float l,vec3 end,vec2 distributed,uint e,bool right_side) {
    vec3 result=vec3(-end.x-distributed.x*x,-end.y-distributed.y*x,
        -end.z+end.y*x+distributed.y*x*x/2.0);
    for(uint b=0u;b<17u;b++) {
        float position=l*float(b)/16.0;
        if(position<x||(right_side&&position<=x)) {
            vec3 point=member_point(e,b);
            result.x-=point.x;result.y-=point.y;
            result.z+=point.y*(x-position)-point.z;
        }
    }
    return result;
}
/* Shared-node collinear members form one continuous physical column. Additional
 * restraints can improve this fixed-free bound, but subdivision cannot. */
float continuous_buckling(uint member) {
    uint g=graph(),mask=1u<<member;
    for(uint pass=0u;pass<16u;pass++) {
        uint previous=mask;
        for(uint candidate=0u;candidate<DC_DATA(g+1u);candidate++) {
            uint m=g+MEMBERS+candidate*8u;
            if((DC_DATA(m+6u)&2u)!=0u||abs(body_cross(eq_direction[member],eq_direction[candidate]))>1e-6) continue;
            for(uint connected=0u;connected<DC_DATA(g+1u);connected++)
                if((mask&(1u<<connected))!=0u&&
                   (eq_a[candidate]==eq_a[connected]||eq_a[candidate]==eq_b[connected]||
                    eq_b[candidate]==eq_a[connected]||eq_b[candidate]==eq_b[connected]))
                    mask|=1u<<candidate;
        }
        if(mask==previous) break;
    }
    float span=0,minimum=1e30;
    for(uint e=0u;e<DC_DATA(g+1u);e++) if((mask&(1u<<e))!=0u) {
        span+=eq_length[e];minimum=min(minimum,eq_k[e].z*eq_length[e]);
    }
    return 9.869604401*minimum/(4.0*span*span);
}
float member_utilization(vec3 force,float depth,uint profile,float buckling) {
    float area=depth,inertia=depth*depth*depth/12.0;
    float bending=abs(force.z)*depth/2.0/inertia;
    float tension=max(force.x/area+bending,0),compression=max(-force.x/area+bending,0);
    float u=max(capacity_ratio(tension,eq_get(profile+3u)),capacity_ratio(compression,eq_get(profile+4u)));
    u=max(u,capacity_ratio(1.5*force.y/area,eq_get(profile+5u)));
    return max(u,capacity_ratio(max(-force.x,0),buckling));
}
void member_stress(uint e) {
    uint g=graph(),m=g+MEMBERS+e*8u,st=member_state(e);
    float l=eq_length[e],depth=float(DC_DATA(m+5u))/65536.0;
    vec3 strain=vec3(0);
    for(uint dof=0u;dof<3u;dof++) {
        strain+=eq_vector(e,uint(eq_a[e])*3u+dof)*eq_q(uint(eq_a[e])*3u+dof);
        strain+=eq_vector(e,uint(eq_b[e])*3u+dof)*eq_q(uint(eq_b[e])*3u+dof);
    }
    vec3 force=eq_k[e]*strain;
    vec2 distributed=eq_weight[e]*vec2(eq_direction[e].y,eq_direction[e].x);
    vec3 end=vec3(-force.x,-force.y,-l*force.y/2.0-force.z)-
        vec3(distributed*l/2.0,distributed.y*l*l/12.0);
    uint profile=g+29000u+DC_DATA(m+4u)*12u;
    float phi=eq_get(profile+1u)*depth*depth/((5.0/6.0)*eq_get(profile+2u)*l*l),den=1+phi;
    for(uint b=0u;b<17u;b++) {
        float r=float(b)/16.0;vec3 point=member_point(e,b);
        vec2 h=vec2(1-3*r*r+2*r*r*r+phi*(1-r),l*(r-2*r*r+r*r*r+phi*r*(1-r)/2))/den;
        vec2 rotation=vec2(6*r*(r-1)/l,1-4*r+3*r*r+phi*(1-r))/den;
        end-=vec3((1-r)*point.x,h.x*point.y+rotation.x*point.z,h.y*point.y+rotation.y*point.z);
    }
    float peak=0,location=0,energy=0;vec3 peak_force=vec3(0);
    float buckling=continuous_buckling(e);
    for(uint b=0u;b<17u;b++) for(uint side=0u;side<2u;side++) {
        float x=l*float(b)/16.0;vec3 result=member_resultant(x,l,end,distributed,e,side!=0u);
        float utilization=member_utilization(result,depth,profile,buckling);
        if(utilization>peak) {peak=utilization;location=x/l;peak_force=result;}
        if(b<16u&&side==1u&&abs(distributed.y)>1e-8) {
            float stationary=x+result.y/distributed.y;
            if(stationary>x&&stationary<x+l/16.0) {
                vec3 interior=member_resultant(stationary,l,end,distributed,e,true);
                float u=member_utilization(interior,depth,profile,buckling);
                if(u>peak) {peak=u;location=stationary/l;peak_force=interior;}
            }
        }
    }
    for(uint b=0u;b<16u;b++) {
        vec3 result=member_resultant(l*(float(b)+.5)/16.0,l,end,distributed,e,true);
        float inertia=depth*depth*depth/12.0;
        energy+=l/32.0*(result.x*result.x/(eq_get(profile+1u)*depth)+
            result.z*result.z/(eq_get(profile+1u)*inertia)+
            result.y*result.y/((5.0/6.0)*eq_get(profile+2u)*depth));
    }
    DC_DATA(st)=DC_DATA(m);DC_DATA(st+1u)=DC_DATA(m+1u);DC_DATA(st+2u)=DC_DATA(state(uint(eq_a[e]))+1u);
    DC_DATA(st+3u)=peak>1?1u:0u;
    for(uint axis=0u;axis<3u;axis++) eq_put(st+4u+axis,peak_force[axis]);
    eq_put(st+7u,peak);eq_put(st+8u,location);eq_put(st+9u,energy);
    if(peak>1) atomicMax(eq_status,1u);
}
void component_budget(uint owner) {
    uint st=component_state(owner),g=graph();vec3 applied=vec3(0),reaction=vec3(0);
    vec2 origin=eq_position(owner);float maximum_residual=0;
    for(uint r=0u;r<eq_count;r++) {
        uint dof=eq_map[r],node=dof/3u,axis=dof%3u;float f=eq_load(dof),support=0;
        for(uint a=0u;a<DC_DATA(g+3u);a++) support+=anchor_reaction(a,dof);
        applied[axis]+=f;reaction[axis]+=support;
        vec2 arm=eq_position(node)-origin;
        if(axis<2u) {
            float lever=axis==0u?-arm.y:arm.x;
            applied.z+=lever*f;reaction.z+=lever*support;
        }
        vec2 physical=eq_physical(dof);
        maximum_residual=max(maximum_residual,abs(physical.x-f)/max(physical.y+abs(f),1));
    }
    DC_DATA(st)=DC_DATA(g+NODES+owner*12u);DC_DATA(st+1u)=eq_status;DC_DATA(st+2u)=eq_count/3u;
    for(uint axis=0u;axis<3u;axis++) {eq_put(st+4u+axis,applied[axis]);eq_put(st+7u+axis,reaction[axis]);}
    eq_put(st+10u,maximum_residual);eq_put(st+11u,eq_minimum);DC_DATA(st+12u)=eq_iterations;
    DC_DATA(st+13u)=eq_active_iterations;DC_DATA(st+14u)=eq_refinement_iterations;
    eq_put(st+15u,eq_geometry_minimum);
}
