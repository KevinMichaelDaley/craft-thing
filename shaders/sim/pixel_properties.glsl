#ifndef DC_PIXEL_PROPERTIES_GLSL
#define DC_PIXEL_PROPERTIES_GLSL

#ifndef DC_PIXEL_PROPERTY_LANES
#define DC_PIXEL_PROPERTY_LANES 128u
#endif
const uint PIXEL_PROPERTY_LANES=DC_PIXEL_PROPERTY_LANES;
shared uint property_mass100[128];
shared uvec2 property_x[128],property_y[128];
shared float property_inertia[128];

uvec2 property_add_wide(uvec2 a,uvec2 b) {
    uint lo=a.x+b.x;
    return uvec2(lo,a.y+b.y+uint(lo<a.x));
}

/* Every lane of the workgroup must participate with the same shape. */
void pixel_mass_properties(Body body,out uint mass100,out vec2 center,out float inertia) {
    uint lane=gl_LocalInvocationIndex,local_mass=0u;
    uvec2 first_x=uvec2(0),first_y=uvec2(0);
    for(uint pixel=lane;pixel<pixel_tile_count(body)*256u;pixel+=PIXEL_PROPERTY_LANES) {
        uint material=pixel_piece_material(body,pixel);
        if(material==0u) continue;
        uint weight=uint(round(100.*body_material_density(material)));
        uvec2 point=uvec2(pixel_piece_cell(body,pixel))*2u+1u;
        local_mass+=weight;
        first_x=property_add_wide(first_x,uvec2(point.x*weight,0));
        first_y=property_add_wide(first_y,uvec2(point.y*weight,0));
    }
    property_mass100[lane]=local_mass;property_x[lane]=first_x;property_y[lane]=first_y;
    for(uint stride=PIXEL_PROPERTY_LANES/2u;stride>0u;stride/=2u) {
        barrier();
        if(lane<stride) {
            property_mass100[lane]+=property_mass100[lane+stride];
            property_x[lane]=property_add_wide(property_x[lane],property_x[lane+stride]);
            property_y[lane]=property_add_wide(property_y[lane],property_y[lane+stride]);
        }
    }
    barrier();
    mass100=property_mass100[0];
    vec2 first=vec2(float(property_x[0].y)*4294967296.+float(property_x[0].x),
                    float(property_y[0].y)*4294967296.+float(property_y[0].x));
    center=first/(2.*float(mass100));
    precise float moment=0.,compensation=0.;
    for(uint pixel=lane;pixel<pixel_tile_count(body)*256u;pixel+=PIXEL_PROPERTY_LANES) {
        uint material=pixel_piece_material(body,pixel);
        if(material==0u) continue;
        float density=body_material_density(material);
        vec2 delta=vec2(pixel_piece_cell(body,pixel))+.5-center;
        precise float value=density*(dot(delta,delta)+1./6.)-compensation;
        precise float sum=moment+value;compensation=(sum-moment)-value;moment=sum;
    }
    property_inertia[lane]=moment;
    for(uint stride=PIXEL_PROPERTY_LANES/2u;stride>0u;stride/=2u) {
        barrier();if(lane<stride) property_inertia[lane]+=property_inertia[lane+stride];
    }
    barrier();inertia=property_inertia[0];
    barrier();
}

#endif
