/* Position differencing can remove the rigid-motion nullspace of a finite
 * rotating assembly. Restore its recorded momentum only when no external
 * manifold, prescribed motion or clipping makes that ledger incomplete. */
uint bond_budget(uint slot) { return structural_graph()+34000u+slot*16u; }
vec2 bond_center(uint slot) { return position(bodies[slot])+bodies[slot].center; }
void prepare_bond_budgets() {
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        uint p=bond_budget(i);data[p]=i;data[p+1u]=0u;data[p+8u]=0u;
    }
    bool any_bond=false;
    for(uint i=0u;i<STRUCTURAL_BOND_CAPACITY;i++) if(bond_live(bond_record(i))) {
        any_bond=true;
        uint p=bond_record(i);data[bond_budget(data[p+1u])+1u]=1u;data[bond_budget(data[p+2u])+1u]=1u;
    }
    data[structural_base()+9u]=uint(any_bond);
    data[structural_base()+10u]=1u;data[structural_base()+11u]=1u;
    if(!any_bond) return;
    for(uint iteration=0u;iteration<BODY_CAPACITY;iteration++) {
        bool changed=false;
        for(uint i=0u;i<STRUCTURAL_BOND_CAPACITY;i++) if(bond_live(bond_record(i))) {
            uint p=bond_record(i),a=bond_budget(data[p+1u]),b=bond_budget(data[p+2u]);
            uint label=min(data[a],data[b]);
            changed=changed||data[a]!=label||data[b]!=label;data[a]=label;data[b]=label;
        }
        if(!changed) break;
    }
    for(uint owner=0u;owner<BODY_CAPACITY;owner++) {
        uint p=bond_budget(owner);
        if(data[p]!=owner||data[p+1u]==0u) continue;
        float mass=0;vec2 center=vec2(0),momentum=vec2(0);bool cold=false,invalid=false;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(data[bond_budget(i)]==owner) {
            Body b=bodies[i];mass+=b.mass;
            center+=b.mass*(getv(bs(i)+6u)+b.center);momentum+=b.mass*velocity(b);
            cold=cold||data[bs(i)+BODY_AWAKE_WORD]==0u;
            invalid=invalid||b.motion_flags!=0u||data[structural_clipped()+i]!=0u;
        }
        center/=mass;float angular=0;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(data[bond_budget(i)]==owner) {
            Body b=bodies[i];
            angular+=b.inertia*b.angular_velocity+
                b.mass*body_cross(getv(bs(i)+6u)+b.center-center,velocity(b));
            if(cold) {
                /* Freeze the whole bonded island without rebasing a hidden
                 * body's stale view-space cache into its world coordinates. */
                if(b.visible!=0u) {
                    publish(b,getv(bs(i)));b.angle=getf(bs(i)+2u);body_cache_bounds(b);
                }
                b.vx_fp=int(data[bs(i)+3u]);b.vy_fp=int(data[bs(i)+4u]);
                b.angular_velocity=getf(bs(i)+5u);bodies[i]=b;
                data[bs(i)+BODY_AWAKE_WORD]=0u;
            }
        }
        putf(p+2u,mass);putv(p+3u,center);putv(p+5u,momentum);putf(p+7u,angular);
        data[p+8u]=uint(cold||invalid);
    }
}
void conserve_isolated_bonds() {
    for(uint c=0u;c<data[cb()];c++) {
        uint p=cb()+12u+c*32u,a=slot(data[p]),b=slot(data[p+1u]);
        uint ca=a<BODY_CAPACITY?data[bond_budget(a)]:BODY_CAPACITY;
        uint cb=b<BODY_CAPACITY?data[bond_budget(b)]:BODY_CAPACITY;
        if(ca==cb) continue;
        if(ca<BODY_CAPACITY) data[bond_budget(ca)+8u]=1u;
        if(cb<BODY_CAPACITY) data[bond_budget(cb)+8u]=1u;
    }
    for(uint owner=0u;owner<BODY_CAPACITY;owner++) {
        uint p=bond_budget(owner);
        if(data[p]!=owner||data[p+1u]==0u||data[p+8u]!=0u) continue;
        float mass=getf(p+2u);vec2 center=vec2(0),momentum=vec2(0);
        bool clipped=false;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(data[bond_budget(i)]==owner) {
            center+=bodies[i].mass*bond_center(i);momentum+=bodies[i].mass*velocity(bodies[i]);
            clipped=clipped||data[structural_clipped()+i]!=0u;
        }
        if(clipped) continue;
        center/=mass;float angular=0,inertia=0;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(data[bond_budget(i)]==owner) {
            Body b=bodies[i];vec2 r=bond_center(i)-center;
            angular+=b.inertia*b.angular_velocity+b.mass*body_cross(r,velocity(b));
            inertia+=b.inertia+b.mass*dot(r,r);
        }
        vec2 target=getv(p+5u),translation=(target-momentum)/mass;
        float rotation=(getf(p+7u)-body_cross(center-getv(p+3u),target)-angular)/inertia;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(data[bond_budget(i)]==owner) {
            Body b=bodies[i];vec2 r=bond_center(i)-center;
            vec2 v=velocity(b)+translation+rotation*vec2(-r.y,r.x);
            b.vx_fp=int(round(v.x*65536.0));b.vy_fp=int(round(v.y*65536.0));
            b.angular_velocity+=rotation;bodies[i]=b;
        }
    }
}
