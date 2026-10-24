#ifndef DC_SOLVER_CONTACTS_GLSL
#define DC_SOLVER_CONTACTS_GLSL
void solve_contact(uint i, bool impulse) {
    uint c = cb() + 12u + i * 32u, s = cs(i), a = DC_CONTACT_DATA(s + 2u), b = DC_CONTACT_DATA(s + 3u);
    if (a == BODY_CAPACITY)
        return;
    Body ba = contact_body(a);
    vec2 ra = body_rotate(contact_getv(s + 4u), ba.angle), rb = contact_getv(s + 6u);
    vec2 pa = position(ba) + ba.center + ra, pb = contact_getv(s + 9u);
    vec2 va = velocity(ba) + ba.angular_velocity * vec2(-ra.y, ra.x), vb = vec2(0);
    uint grain_ref=grain_reference(c);
    float creep=contact_getf(s+12u);
    float mb = 0.0, ib = 0.0;
    if(grain_ref!=0u) {
        Particle p=particles[grain_ref-1u];
        pb=grain_position(c,grain_ref-1u)+rb;
        vb=vec2(p.vx_fp,p.vy_fp)/65536.0;
        mb=p.mass_fp!=0u?65536.0/float(p.mass_fp):0.0;
        if(creep>=0.0) {
            mb=0.0;
            pb+=contact_getf(s+13u)*contact_getv(c+8u);
            pb.y+=creep*DT;vb.y=creep;
        }
    }
    if (b != BODY_CAPACITY) {
        Body bb = contact_body(b);
        rb = body_rotate(rb, bb.angle);
        pb = position(bb) + bb.center + rb;
        vb = velocity(bb) + bb.angular_velocity * vec2(-rb.y, rb.x);
        mb = bb.inverse_mass;
        ib = bb.inverse_inertia;
    }
    vec2 n = contact_getv(c + 8u), t = vec2(-n.y, n.x);
    float an = body_cross(ra, n), bn = body_cross(rb, n), at = body_cross(ra, t),
          bt = body_cross(rb, t);
    float wn = ba.inverse_mass + mb + ba.inverse_inertia * an * an + ib * bn * bn;
    float wt = ba.inverse_mass + mb + ba.inverse_inertia * at * at + ib * bt * bt;
    if (wn <= 0.0)
        return;
    float scale = weight(a, b,(grain_ref!=0u&&creep<0.0)||DC_CONTACT_DATA(s+11u)==0u), dn = 0.0, dt = 0.0;
    if(grain_ref!=0u) scale=min(scale,0.8/float(max(grain_scratch[(grain_ref-1u)*4u+3u],1u)));
    if (impulse) {
        if (dot(pa - pb, n) > 1.0 / 65536.0 && contact_getf(s) == 0.0)
            return;
        float pre = contact_getf(s + 8u), target = pre < -0.5 ? -contact_getf(c + 12u) * pre : 0.0;
        dn = max(0.0, (target - dot(va - vb, n)) / wn) * scale;
        float limit = contact_getf(c + 11u) * (contact_getf(s) / DT + dn);
        if (wt > 0.0)
            dt = clamp(-dot(va - vb, t) / wt, -limit, limit) * scale;
    } else {
        float alpha = creep>=0.0?0.0:contact_getf(c + 13u) / (DT * DT), lambda = contact_getf(s);
        dn = (max(0.0, lambda + (-dot(pa - pb, n) - alpha * lambda) / (wn + alpha)) - lambda) *
             scale;
        contact_putf(s, lambda + dn);
        vec2 start_a = CACHE_CONTACT_STARTS?contact_getv(s+14u):contact_start_anchor(a,ba,contact_getv(s+4u));
        vec2 start_b = contact_getv(s + 9u);
        if (b != BODY_CAPACITY)
            start_b =
                CACHE_CONTACT_STARTS?contact_getv(s+9u):contact_start_anchor(b,DC_BODY(b),contact_getv(s+6u));
        float tangent_error = dot((pa - start_a) - (pb - start_b), t);
        float lt = contact_getf(s + 1u), limit = contact_getf(c + 11u) * max(0.0, lambda + dn);
        if (wt > 0.0)
            dt = (clamp(lt - tangent_error / wt, -limit, limit) - lt) * scale;
        contact_putf(s + 1u, lt + dt);
    }
    if(SKIP_ZERO_REDUCTIONS&&dn==0.0&&dt==0.0) return;
    vec2 force = dn * n + dt * t;
    vec2 applied_impulse=force/(impulse?1.0:DT);
    float incoming=contact_getf(s+8u),normal_impulse=max(dn,0)/(impulse?1.0:DT);
    impact_capture(a,b,DC_DATA(c+5u),DC_DATA(c+6u),n,incoming,normal_impulse,wn,contact_getf(c+12u));
    impact_capture(b,a,DC_DATA(c+6u),DC_DATA(c+5u),-n,incoming,normal_impulse,wn,contact_getf(c+12u));
    structural_capture(a,b,pa,applied_impulse);
    structural_capture(b,a,pb,-applied_impulse);
    if(any(greaterThan(abs(vec3(ba.inverse_mass*force,ba.inverse_inertia*(an*dn+at*dt))),vec3(2))))
        atomicOr(DC_BODY_CLIPPED(a),1u);
    if(b<BODY_CAPACITY&&any(greaterThan(abs(vec3(mb*force,ib*(bn*dn+bt*dt))),vec3(2))))
        atomicOr(DC_BODY_CLIPPED(b),1u);
    add_correction(a, ba.inverse_mass * force, ba.inverse_inertia * (an * dn + at * dt));
    add_correction(b, -mb * force, -ib * (bn * dn + bt * dt));
    if(grain_ref!=0u) grain_correction(grain_ref-1u,-mb*force);
}
#endif
