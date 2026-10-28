#ifdef DC_RUNTIME_BODY_STORAGE
layout(buffer_reference,std430,buffer_reference_align=4) buffer RevoluteRecords {uint words[];};
float joint_float(RevoluteRecords joints,uint at) {return uintBitsToFloat(joints.words[at]);}
void solve_revolute(uint index,uint phase) {
    if(index>=DC_RUNTIME_WORK(98u)||DC_DATA(sb()+3u)!=0u) return;
    RevoluteRecords joints=RevoluteRecords(uvec2(DC_RUNTIME_WORK(96u),DC_RUNTIME_WORK(97u)));
    uint p=index*24u;if(joints.words[p+1u]==0u) return;
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
        joints.words[p+20u]=0u;joints.words[p+21u]=0u;joints.words[p+22u]=0u;
        atomicAdd(SOLVER_BODY_WORD(ai,20u),uint(SUM_SCALE));
        atomicAdd(SOLVER_BODY_WORD(bi,20u),uint(SUM_SCALE));
        return;
    }
    Body a=contact_body(ai),b=contact_body(bi);
    vec2 ra=body_rotate(vec2(joint_float(joints,p+8u),joint_float(joints,p+9u))-a.center,a.angle);
    vec2 rb=body_rotate(vec2(joint_float(joints,p+10u),joint_float(joints,p+11u))-b.center,b.angle);
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
    uint flags=joints.words[p+12u];
    float relative=wrap(b.angle-a.angle),angular_mass=a.inverse_inertia+b.inverse_inertia;
    float lower=joint_float(joints,p+13u),upper=joint_float(joints,p+14u),angular_impulse=0.;
    if(angular_mass>0.) {
        float limit_error=relative-clamp(relative,lower,upper);
        float omega=b.angular_velocity-a.angular_velocity;
        if((flags&1u)!=0u) {
            if(phase==23u) angular_impulse=-limit_error/angular_mass;
            else if((relative<=lower+.0001&&omega<0.)||(relative>=upper-.0001&&omega>0.))
                angular_impulse=-omega/angular_mass;
        }
        if(phase==24u) {
            if((flags&2u)!=0u) {
                float stiffness=joint_float(joints,p+15u),error=wrap(relative-joint_float(joints,p+16u));
                angular_impulse-=DT*stiffness*(error+DT*omega)/(1.+DT*DT*stiffness*angular_mass);
            }
            float friction=joint_float(joints,p+17u)*DT;
            angular_impulse+=clamp(-omega/angular_mass,-friction,friction);
        }
        angular_impulse*=weight(ai,bi,false);
    }
    float max_force=joint_float(joints,p+18u),max_torque=joint_float(joints,p+19u);
    if(phase==23u) {
        vec2 accumulated=vec2(joint_float(joints,p+20u),joint_float(joints,p+21u))+impulse;
        float angular_total=joint_float(joints,p+22u)+angular_impulse;
        if((max_force>0.&&length(accumulated)>max_force*DT*DT)||
           (max_torque>0.&&abs(angular_total)>max_torque*DT*DT)) {
            joints.words[p+1u]=0u;return;
        }
        joints.words[p+20u]=floatBitsToUint(accumulated.x);
        joints.words[p+21u]=floatBitsToUint(accumulated.y);
        joints.words[p+22u]=floatBitsToUint(angular_total);
    } else if(max_torque>0.) angular_impulse=clamp(angular_impulse,-max_torque*DT,max_torque*DT);
    vec2 point_a=position(a)+a.center+ra,point_b=position(b)+b.center+rb;
    float conversion=phase==23u?1./DT:1.;
    structural_capture(ai,bi,point_a,-impulse*conversion);
    structural_capture(bi,ai,point_b,impulse*conversion);
    if(angular_impulse!=0.) {
        vec2 couple=vec2(0,angular_impulse*conversion),offset=vec2(.5,0);
        structural_capture(ai,bi,point_a+offset,-couple);
        structural_capture(ai,bi,point_a-offset,couple);
        structural_capture(bi,ai,point_b+offset,couple);
        structural_capture(bi,ai,point_b-offset,-couple);
    }
    add_correction(ai,-a.inverse_mass*impulse,-a.inverse_inertia*body_cross(ra,impulse));
    add_correction(bi,b.inverse_mass*impulse,b.inverse_inertia*body_cross(rb,impulse));
    add_correction(ai,vec2(0),-a.inverse_inertia*angular_impulse);
    add_correction(bi,vec2(0),b.inverse_inertia*angular_impulse);
}
#else
void solve_revolute(uint index,uint phase) {}
#endif
