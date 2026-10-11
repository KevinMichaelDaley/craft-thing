#include "structural_bonds.glsl"
void solve_bonds(uint phase) {
    if(gl_GlobalInvocationID.x!=0u||data[sb()+3u]!=0u) return;
    for(uint i=0u;i<STRUCTURAL_BOND_CAPACITY;i++) {
        uint p=bond_record(i);
        if(!bond_live(p)) continue;
        if(phase==14u) {
            for(uint axis=0u;axis<3u;axis++) data[p+15u+axis]=0u;
            continue;
        }
        uint ai=data[p+1u],bi=data[p+2u];
        if(data[bs(ai)+BODY_AWAKE_WORD]==0u||data[bs(bi)+BODY_AWAKE_WORD]==0u) continue;
        Body a=bodies[ai],b=bodies[bi];
        for(uint axis=0u;axis<3u;axis++) {
            float stiffness=getf(p+12u+axis);
            if(stiffness<=0) continue;
            vec2 ra=body_rotate(getv(p+6u),a.angle),rb=body_rotate(getv(p+8u),b.angle);
            float angle=a.angle+getf(p+11u)+float(axis)*1.5707963267948966;
            vec2 n=vec2(cos(angle),sin(angle));
            float ja=axis==2u?-1.0:-body_cross(ra,n);
            float jb=axis==2u?1.0:body_cross(rb,n);
            float mass=a.inverse_inertia*ja*ja+b.inverse_inertia*jb*jb;
            if(axis<2u) mass+=a.inverse_mass+b.inverse_mass;
            if(mass<=0) continue;
            float c;
            if(phase==13u) {
                c=ja*a.angular_velocity+jb*b.angular_velocity;
                if(axis<2u) c+=dot(n,velocity(b)-velocity(a));
            } else if(axis==2u) c=wrap(b.angle-a.angle-getf(p+10u));
            else c=dot(n,position(b)+b.center+rb-position(a)-a.center-ra);
            float compliance=phase==13u?0:3600.0/(stiffness*DT*DT);
            float previous=getf(p+15u+axis);
            float lambda=-(c+compliance*previous)/(mass+compliance);
            if(phase!=13u) putf(p+15u+axis,previous+lambda);
            if(phase==13u) {
                vec2 va=velocity(a),vb=velocity(b);
                if(axis<2u) {va-=a.inverse_mass*n*lambda;vb+=b.inverse_mass*n*lambda;}
                a.vx_fp=int(round(va.x*65536.0));a.vy_fp=int(round(va.y*65536.0));
                b.vx_fp=int(round(vb.x*65536.0));b.vy_fp=int(round(vb.y*65536.0));
                a.angular_velocity+=a.inverse_inertia*ja*lambda;
                b.angular_velocity+=b.inverse_inertia*jb*lambda;
            } else {
                if(axis<2u) {
                    publish(a,position(a)-a.inverse_mass*n*lambda);
                    publish(b,position(b)+b.inverse_mass*n*lambda);
                }
                a.angle=wrap(a.angle+a.inverse_inertia*ja*lambda);
                b.angle=wrap(b.angle+b.inverse_inertia*jb*lambda);
            }
        }
        bodies[ai]=a;bodies[bi]=b;
    }
}
