struct StructuralParticle {
    int x_fp,y_fp,vx_fp,vy_fp;
    float f00,f01,f10,f11;
    uint id_lo,id_hi,mass_fp,grain_fp,material,flags;
};
layout(set=0,binding=3,std430) buffer DebrisOccupancy { uint debris_occupancy[]; };
layout(set=0,binding=19,std430) buffer DebrisParticles { StructuralParticle debris_particles[]; };
layout(set=0,binding=20,std430) buffer DebrisCounts { uint debris_counts[]; };

/* Cell clipping creates quadrature volumes, not raster-box approximations.
 * The two passes reserve every exact overlap before changing either owner. */
float debris_cell(Body b,ivec2 cell,out vec2 center) {
    vec2 polygon[8],next[8];uint count=4u;
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0;
    for(uint i=0u;i<4u;i++) polygon[i]=origin+body_vertex(b,i);
    for(uint plane=0u;plane<4u;plane++) {
        uint axis=plane/2u,n=0u;bool upper=(plane&1u)!=0u;
        float boundary=float(cell[axis])+float(upper);
        for(uint i=0u;i<count;i++) {
            vec2 a=polygon[i],z=polygon[(i+1u)%count];
            float da=upper?boundary-a[axis]:a[axis]-boundary;
            float dz=upper?boundary-z[axis]:z[axis]-boundary;
            if(da>=0) next[n++]=a;
            if((da>0&&dz<0)||(da<0&&dz>0)) next[n++]=mix(a,z,da/(da-dz));
        }
        count=n;for(uint i=0u;i<count;i++) polygon[i]=next[i];
        if(count<3u) {center=vec2(0);return 0;}
    }
    float twice=0;vec2 moment=vec2(0);
    for(uint i=0u;i<count;i++) {
        vec2 a=polygon[i]-vec2(cell),z=polygon[(i+1u)%count]-vec2(cell);
        float cross=body_cross(a,z);twice+=cross;moment+=(a+z)*cross;
    }
    center=vec2(cell)+moment/max(3*twice,1e-20);
    return max(twice*.5,0);
}
bool crumble_member(uint member) {
    uint g=graph(),m=g+MEMBERS+member*8u,st=member_state(member);
    uint serial=DC_DATA(base()+14u)+1u;
    if(serial==0u||serial>0x1fffffffu) return false;
    Body b=DC_BODY(DC_DATA(m+7u));
    uint debris_material=DC_DATA(m+4u)==0u?5u:6u;
    vec2 low,high;body_bounds(b,low,high);
    vec2 origin=vec2(b.x_fp,b.y_fp)/65536.0;
    ivec2 begin=ivec2(floor(origin+low)),end=ivec2(ceil(origin+high));
    float area=0;
    uint profile=g+29000u+DC_DATA(m+4u)*12u;
    float work=eq_get(profile+6u)*float(DC_DATA(m+5u))/65536.0;
    if(eq_get(st+9u)+1e-4<work) return false;
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 cell=ivec2(x,y);vec2 center;float volume=debris_cell(b,cell,center);
        if(volume<1e-7) continue;
        uint c;if(!cell_ref(cell,c)) return false;
        uint material=cells[c].x&65535u;
        if(material!=0u&&material!=3u&&material!=debris_material) return false;
        uint p=(c/4096u)*8192u+c%4096u;
        if(debris_particles[p].mass_fp!=0u&&debris_particles[p+4096u].mass_fp!=0u) return false;
        area+=volume;
    }
    if(area<=0) return false;
    uint total=uint(round(b.mass*65536.0)),written=0u;float accumulated=0;
    if(total==0u) return false;
    for(int y=begin.y;y<end.y;y++) for(int x=begin.x;x<end.x;x++) {
        ivec2 cell=ivec2(x,y);vec2 center;float volume=debris_cell(b,cell,center);
        if(volume<1e-7) continue;
        uint c;cell_ref(cell,c);uint p=(c/4096u)*8192u+c%4096u;
        if(debris_particles[p].mass_fp!=0u) p+=4096u;
        accumulated+=volume;
        uint cumulative=uint(round(float(total)*min(accumulated/area,1.0)));
        uint mass=cumulative-written;written=cumulative;
        if(mass==0u) continue;
        StructuralParticle particle;
        particle.x_fp=int(round((float(x%64)+center.x-float(x))*65536.0));
        particle.y_fp=int(round((float(y%64)+center.y-float(y))*65536.0));
        particle.vx_fp=0;particle.vy_fp=0;
        particle.f00=1;particle.f01=0;particle.f10=0;particle.f11=1;
        particle.id_lo=0x80000000u|((c%4096u)+1u);
        particle.id_hi=serial;
        particle.mass_fp=mass;
        particle.grain_fp=uint(round(min(.35,sqrt(volume/3.14159265))*65536.0));
        particle.material=debris_material;particle.flags=0u;
        debris_particles[p]=particle;debris_counts[c/4096u]++;
        cells[c].x=(cells[c].x&0xffff0000u)|debris_material;
        uint view=uint(y)*params.width+uint(x);
        if(debris_occupancy[view]==b.id) debris_occupancy[view]=0u;
        if(debris_occupancy[params.width*params.height+view]==b.id)
            debris_occupancy[params.width*params.height+view]=0u;
    }
    b.enabled=0u;b.reserved0=0x53545232u;b.reserved1=DC_DATA(m+1u);
    DC_BODY(DC_DATA(m+7u))=b;
    uint record=body_fragment_record(DC_DATA(m+7u));
    DC_SOLVER_DATA(record)=b.id;DC_SOLVER_DATA(record+1u)=DC_DATA(m+1u);
    DC_SOLVER_DATA(record+2u)=serial;DC_SOLVER_DATA(record+3u)=2u;
    DC_DATA(base()+14u)=serial;
    DC_DATA(m+6u)|=STRUCTURAL_RETIRED;DC_DATA(st+3u)=5u;
    eq_put(st+11u,work);
    eq_put(st+12u,max(eq_get(st+9u)-work,0));
    DC_DATA(base()+16u)=(params.width+15u)/16u;
    DC_DATA(base()+17u)=(params.height+15u)/16u;DC_DATA(base()+18u)=1u;
    return true;
}
