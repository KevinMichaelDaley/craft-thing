#ifdef DC_RUNTIME_BODY_STORAGE
layout(buffer_reference,std430,buffer_reference_align=4) buffer RevoluteRecords {uint words[];};
float joint_float(RevoluteRecords joints,uint at) {return uintBitsToFloat(joints.words[at]);}
void solve_revolute(uint index,uint phase) {
    if(index>=DC_RUNTIME_WORK(98u)||DC_DATA(sb()+3u)!=0u) return;
    RevoluteRecords joints=RevoluteRecords(uvec2(DC_RUNTIME_WORK(96u),DC_RUNTIME_WORK(97u)));
    uint p=index*16u;if(joints.words[p+1u]==0u) return;
    uint ai=joints.words[p+2u],bi=joints.words[p+5u];
    bool valid=ai<BODY_CAPACITY&&bi<BODY_CAPACITY;
    if(valid) {
        BodyStorageOwner ao=DC_BODY_OWNER(runtime_body_root,ai),bo=DC_BODY_OWNER(runtime_body_root,bi);
        valid=ao.state==DC_BODY_OWNER_LIVE&&bo.state==DC_BODY_OWNER_LIVE&&
            ao.generation==joints.words[p+3u]&&bo.generation==joints.words[p+6u]&&
            ao.runtime_id==joints.words[p+4u]&&bo.runtime_id==joints.words[p+7u];
    }
    if(!valid) {joints.words[p+1u]=0u;return;}
    if(SOLVER_BODY_WORD(ai,BODY_AWAKE_WORD)==0u||SOLVER_BODY_WORD(bi,BODY_AWAKE_WORD)==0u) return;
    if(phase==22u) {
        atomicAdd(SOLVER_BODY_WORD(ai,20u),uint(SUM_SCALE));
        atomicAdd(SOLVER_BODY_WORD(bi,20u),uint(SUM_SCALE));
        return;
    }
    Body a=contact_body(ai),b=contact_body(bi);
    vec2 ra=body_rotate(vec2(joint_float(joints,p+8u),joint_float(joints,p+9u)),a.angle);
    vec2 rb=body_rotate(vec2(joint_float(joints,p+10u),joint_float(joints,p+11u)),b.angle);
    vec2 delta=position(b)+b.center+rb-position(a)-a.center-ra;
    if(phase==24u) delta=velocity(b)+b.angular_velocity*vec2(-rb.y,rb.x)-
        velocity(a)-a.angular_velocity*vec2(-ra.y,ra.x);
    float linear=a.inverse_mass+b.inverse_mass;
    float xx=linear+a.inverse_inertia*ra.y*ra.y+b.inverse_inertia*rb.y*rb.y;
    float yy=linear+a.inverse_inertia*ra.x*ra.x+b.inverse_inertia*rb.x*rb.x;
    float xy=-a.inverse_inertia*ra.x*ra.y-b.inverse_inertia*rb.x*rb.y;
    float determinant=xx*yy-xy*xy;
    if(determinant<=1e-20) return;
    vec2 impulse=-vec2(yy*delta.x-xy*delta.y,xx*delta.y-xy*delta.x)/determinant;
    impulse*=weight(ai,bi,false);
    add_correction(ai,-a.inverse_mass*impulse,-a.inverse_inertia*body_cross(ra,impulse));
    add_correction(bi,b.inverse_mass*impulse,b.inverse_inertia*body_cross(rb,impulse));
}
#else
void solve_revolute(uint index,uint phase) {}
#endif
