shared vec4 body_thermal_moments[64];
shared uint body_thermal_counts[64];
shared uint body_thermal_owned[64];
float body_thermal_piece_mass(Body body,uint slot,uint piece,uint material,out bool reactive) {
    uvec2 h=fr_body_reactive_handle(body,slot,piece);reactive=h.y!=0u;
    if(!reactive) return body_material_density(material);
    Words e=Words(reactive_entry_address(uvec2(root[4],root[5]),h.x));float mass=0.;
    for(uint species=9u;species<=16u;species++) mass+=float(e.words[FR_ENTRY_STATE+species])/65536.;
    return mass;
}
void body_thermal_mass(uint slot,Body body) {
    uint lane=gl_LocalInvocationID.x,count=0u,owned=0u;vec4 moments=vec4(0);
    for(uint piece=lane;piece<body_pieces(body);piece+=64u) {
        uint material=pixel_piece_material(body,piece);if(material==0u) continue;
        bool reactive;float mass=body_thermal_piece_mass(body,slot,piece,material,reactive);owned+=reactive?1u:0u;
        vec2 p=vec2(pixel_piece_cell(body,piece))+.5;
        moments+=vec4(mass,mass*p,0.);count++;
    }
    body_thermal_moments[lane]=moments;body_thermal_counts[lane]=count;body_thermal_owned[lane]=owned;
    for(uint stride=32u;stride>0u;stride/=2u) {
        barrier();if(lane<stride) {body_thermal_moments[lane]+=body_thermal_moments[lane+stride];
            body_thermal_counts[lane]+=body_thermal_counts[lane+stride];body_thermal_owned[lane]+=body_thermal_owned[lane+stride];}
    }
    barrier();moments=body_thermal_moments[0];
    if(moments.x<=0.||body_thermal_counts[0]==0u||body_thermal_owned[0]==0u) return;
    vec2 center=moments.yz/moments.x;float moment=0.;
    for(uint piece=lane;piece<body_pieces(body);piece+=64u) {
        uint material=pixel_piece_material(body,piece);if(material==0u) continue;
        bool reactive;float mass=body_thermal_piece_mass(body,slot,piece,material,reactive);
        vec2 delta=vec2(pixel_piece_cell(body,piece))+.5-center;moment+=mass*(dot(delta,delta)+1./6.);
    }
    body_thermal_moments[lane].w=moment;
    for(uint stride=32u;stride>0u;stride/=2u) {barrier();if(lane<stride) body_thermal_moments[lane].w+=body_thermal_moments[lane+stride].w;}
    barrier();if(lane!=0u) return;
    float inertia=max(1e-12,body_thermal_moments[0].w);
    if(abs(body.mass-moments.x)<1e-7&&length(body.center-center)<1e-7&&(body.motion_flags&DC_BODY_MOTION_REACTIVE_MASS)!=0u) return;
    body.motion_flags|=DC_BODY_MOTION_REACTIVE_MASS;
    vec2 shift=body_rotate(center-body.center,body.angle),origin=body.center+shift-center;
    body.x_fp+=int(round(origin.x*65536.));body.y_fp+=int(round(origin.y*65536.));
    body.vx_fp+=int(round(-body.angular_velocity*shift.y*65536.));
    body.vy_fp+=int(round(body.angular_velocity*shift.x*65536.));
    body.center=center;body.mass=moments.x;body.inertia=inertia;
    vec2 low=pixel_low(body),high=pixel_high(body);
    body.radius=max(max(length(low-center),length(vec2(high.x,low.y)-center)),
        max(length(vec2(low.x,high.y)-center),length(high-center)));
    body.density=moments.x/float(body_thermal_counts[0]);
    body.inverse_mass=(body.motion_flags&1u)==0u?1./body.mass:0.;
    body.inverse_inertia=(body.motion_flags&3u)==0u?1./body.inertia:0.;
    body.motion_ready=1u;body.rest_ticks=0u;DC_BODY(slot)=body;
}
