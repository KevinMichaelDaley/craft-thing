/* Position differencing can remove the rigid-motion nullspace of a finite
 * rotating assembly. Restore its recorded momentum only when no external
 * manifold, prescribed motion or clipping makes that ledger incomplete. */
uint bond_budget(uint slot) {
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_BODY_STATE(slot)+32u;
#else
    return structural_graph()+34000u+slot*16u;
#endif
}
vec2 bond_center(uint slot) { return position(DC_BODY(slot))+DC_BODY(slot).center; }
void prepare_bond_budgets() {
    for(uint i=0u;i<BODY_CAPACITY;i++) {
        uint p=bond_budget(i);DC_SOLVER_DATA(p)=i;DC_SOLVER_DATA(p+1u)=0u;DC_SOLVER_DATA(p+8u)=0u;
    }
    bool any_bond=false;
    for(uint i=0u;i<STRUCTURAL_BOND_CAPACITY;i++) if(bond_live(bond_record(i))) {
        any_bond=true;
        uint p=bond_record(i);DC_SOLVER_DATA(bond_budget(DC_SOLVER_DATA(p+1u))+1u)=1u;DC_SOLVER_DATA(bond_budget(DC_SOLVER_DATA(p+2u))+1u)=1u;
    }
    DC_SOLVER_DATA(structural_base()+9u)=uint(any_bond);
    DC_SOLVER_DATA(structural_base()+10u)=1u;DC_SOLVER_DATA(structural_base()+11u)=1u;
    if(!any_bond) return;
    for(uint iteration=0u;iteration<BODY_CAPACITY;iteration++) {
        bool changed=false;
        for(uint i=0u;i<STRUCTURAL_BOND_CAPACITY;i++) if(bond_live(bond_record(i))) {
            uint p=bond_record(i),a=bond_budget(DC_SOLVER_DATA(p+1u)),b=bond_budget(DC_SOLVER_DATA(p+2u));
            uint label=min(DC_SOLVER_DATA(a),DC_SOLVER_DATA(b));
            changed=changed||DC_SOLVER_DATA(a)!=label||DC_SOLVER_DATA(b)!=label;DC_SOLVER_DATA(a)=label;DC_SOLVER_DATA(b)=label;
        }
        if(!changed) break;
    }
    for(uint owner=0u;owner<BODY_CAPACITY;owner++) {
        uint p=bond_budget(owner);
        if(DC_SOLVER_DATA(p)!=owner||DC_SOLVER_DATA(p+1u)==0u) continue;
        float mass=0;vec2 center=vec2(0),momentum=vec2(0);bool cold=false,invalid=false;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_SOLVER_DATA(bond_budget(i))==owner) {
            Body b=DC_BODY(i);mass+=b.mass;
            center+=b.mass*(getv(bs(i)+6u)+b.center);momentum+=b.mass*velocity(b);
            cold=cold||DC_SOLVER_DATA(bs(i)+BODY_AWAKE_WORD)==0u;
            invalid=invalid||b.motion_flags!=0u||DC_BODY_CLIPPED(i)!=0u;
        }
        center/=mass;float angular=0;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_SOLVER_DATA(bond_budget(i))==owner) {
            Body b=DC_BODY(i);
            angular+=b.inertia*b.angular_velocity+
                b.mass*body_cross(getv(bs(i)+6u)+b.center-center,velocity(b));
            if(cold) {
                /* Freeze the whole bonded island without rebasing a hidden
                 * body's stale view-space cache into its world coordinates. */
                if(b.visible!=0u) {
                    publish(b,getv(bs(i)));b.angle=getf(bs(i)+2u);body_cache_bounds(b);
                }
                b.vx_fp=int(DC_SOLVER_DATA(bs(i)+3u));b.vy_fp=int(DC_SOLVER_DATA(bs(i)+4u));
                b.angular_velocity=getf(bs(i)+5u);DC_BODY(i)=b;
                DC_SOLVER_DATA(bs(i)+BODY_AWAKE_WORD)=0u;
            }
        }
        putf(p+2u,mass);putv(p+3u,center);putv(p+5u,momentum);putf(p+7u,angular);
        DC_SOLVER_DATA(p+8u)=uint(cold||invalid);
    }
}
void conserve_isolated_bonds() {
    for(uint c=0u;c<DC_SOLVER_DATA(cb());c++) {
        uint p=cb()+12u+c*32u,a=slot(DC_SOLVER_DATA(p)),b=slot(DC_SOLVER_DATA(p+1u));
        uint ca=a<BODY_CAPACITY?DC_SOLVER_DATA(bond_budget(a)):BODY_CAPACITY;
        uint cb=b<BODY_CAPACITY?DC_SOLVER_DATA(bond_budget(b)):BODY_CAPACITY;
        if(ca==cb) continue;
        if(ca<BODY_CAPACITY) DC_SOLVER_DATA(bond_budget(ca)+8u)=1u;
        if(cb<BODY_CAPACITY) DC_SOLVER_DATA(bond_budget(cb)+8u)=1u;
    }
    for(uint owner=0u;owner<BODY_CAPACITY;owner++) {
        uint p=bond_budget(owner);
        if(DC_SOLVER_DATA(p)!=owner||DC_SOLVER_DATA(p+1u)==0u||DC_SOLVER_DATA(p+8u)!=0u) continue;
        float mass=getf(p+2u);vec2 center=vec2(0),momentum=vec2(0);
        bool clipped=false;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_SOLVER_DATA(bond_budget(i))==owner) {
            center+=DC_BODY(i).mass*bond_center(i);momentum+=DC_BODY(i).mass*velocity(DC_BODY(i));
            clipped=clipped||DC_BODY_CLIPPED(i)!=0u;
        }
        if(clipped) continue;
        center/=mass;float angular=0,inertia=0;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_SOLVER_DATA(bond_budget(i))==owner) {
            Body b=DC_BODY(i);vec2 r=bond_center(i)-center;
            angular+=b.inertia*b.angular_velocity+b.mass*body_cross(r,velocity(b));
            inertia+=b.inertia+b.mass*dot(r,r);
        }
        vec2 target=getv(p+5u),translation=(target-momentum)/mass;
        float rotation=(getf(p+7u)-body_cross(center-getv(p+3u),target)-angular)/inertia;
        for(uint i=0u;i<BODY_CAPACITY;i++) if(DC_SOLVER_DATA(bond_budget(i))==owner) {
            Body b=DC_BODY(i);vec2 r=bond_center(i)-center;
            vec2 v=velocity(b)+translation+rotation*vec2(-r.y,r.x);
            b.vx_fp=int(round(v.x*65536.0));b.vy_fp=int(round(v.y*65536.0));
            b.angular_velocity+=rotation;DC_BODY(i)=b;
        }
    }
}
