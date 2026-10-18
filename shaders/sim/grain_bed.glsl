/* MPM grid scratch is dead during rigid solving. Parallel pointer doubling
   transmits column support to terrain without a CPU walk or a depth limit. */
layout(set=0,binding=23,std430) buffer GrainBed { vec4 bed[]; };
layout(set=0,binding=24,std430) buffer GrainBedScratch { vec4 bed_scratch[]; };
const uint GRAIN_BED_INIT=18u,GRAIN_BED_JUMP=19u;
uint bed_steps() {return uint(findMSB(max(params.height,2u)-1u))+1u;}
vec4 bed_read(uint index) {return (bed_steps()&1u)==0u?bed[index]:bed_scratch[index];}
uint bed_material(ivec2 cell) {
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) return 0u;
    uint page=pages[(uint(cell.y)/64u)*((params.width+63u)/64u)+uint(cell.x)/64u];
    return page==0u?0u:grain_cells[(page-1u)*4096u+uint(cell.y%64)*64u+uint(cell.x%64)].x&65535u;
}
bool bed_solid(uint material) {return material!=0u&&material!=3u;}
void grain_bed_phase(uint i) {
    if(i>=params.width*params.height) return;
    if(PHASE==GRAIN_BED_JUMP) {
        bool odd=((params.mode>>8u)&1u)!=0u;
        vec4 value=odd?bed_scratch[i]:bed[i];
        if(value.x>=0.0) {
            uint next=uint(value.x);
            vec4 below=odd?bed_scratch[next]:bed[next];
            value=vec4(below.x,value.yz+below.yz,below.w);
        }
        if(odd) bed[i]=value;else bed_scratch[i]=value;
        return;
    }
    uvec2 cell=uvec2(i%params.width,i/params.width);
    uint page=pages[(cell.y/64u)*((params.width+63u)/64u)+cell.x/64u];
    vec4 value=vec4(-1,0,0,0);
    if(page!=0u) {
        uint local=(cell.y%64u)*64u+cell.x%64u;
        uint material=grain_cells[(page-1u)*4096u+local].x&65535u;
        if(material==1u||material==7u||material==8u||material==9u) value.w=1.0;
        else if(material==2u||material==4u||material==5u||material==6u||material==10u) {
            uint p=(page-1u)*8192u+local;
            float mass=float(particles[p].mass_fp+particles[p+4096u].mass_fp)/65536.0;
            if(mass>0.0) {
                float next=-1.0;
                /* The MPM kernel spans neighboring nodes: sparse small sand
                   samples still transmit compression across that footprint. */
                for(int dy=1;dy<=2&&next<0.0;dy++) for(int side=0;side<3;side++) {
                    int dx=side==0?0:side==1?-1:1;
                    ivec2 below=ivec2(cell)+ivec2(dx,dy);
                    if(next<0.0&&bed_solid(bed_material(below))) next=float(uint(below.y)*params.width+uint(below.x));
                }
                value=vec4(next,mass,1,0);
            }
        }
    }
    bed[i]=value;
}
/* Drained frictional bearing capacity scales with loaded width and supported
   depth. Exceeding yield introduces bounded viscoplastic settlement, rather
   than turning the entire bed into disconnected free particles. */
float grain_bed_creep(Body body,uint c,uint reference) {
    Particle p=particles[reference-1u];
    /* Thin leaf flakes bend and evacuate; they are not a mineral bearing bed. */
    if(p.material==10u) return -1.0;
    vec2 center=grain_position(c,reference-1u);
    vec2 local_point=body_local_point(body,center-position(body));
    uint piece=(data[c+3u]>>28u)&3u;
    float interior_margin=float(p.grain_fp)/65536.0+0.25;
    if(pixel_body(body)) {
        vec2 outward=body_rotate(-getv(c+8u),-body.angle)*interior_margin;
        if(pixel_material(body,ivec2(floor(local_point)))!=0u&&
           pixel_material(body,ivec2(floor(local_point+outward)))!=0u) return -1.0;
    } else {
        bool inside=true;
        for(uint v=0u;v<piece_vertices(body,piece);v++) {
            vec2 a=piece_rest_vertex(body,piece,v),b=piece_rest_vertex(body,piece,(v+1u)%piece_vertices(body,piece));
            if(body_cross(b-a,local_point-a)<interior_margin*length(b-a)) inside=false;
        }
        if(inside) return -1.0;
    }
    uint local=(reference-1u)%4096u;
    vec2 point=center-vec2(p.x_fp,p.y_fp)/65536.0+vec2(local%64u,local/64u)+vec2(0.5);
    ivec2 cell=ivec2(floor(point));
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,ivec2(params.width,params.height)))) return -1.0;
    vec4 support=bed_read(uint(cell.y)*params.width+uint(cell.x));
    if(support.w==0.0||support.z<1.0) return -1.0;
    float friction=p.material==5u?0.75:p.material==4u?0.65:p.material==2u?0.58:0.3;
    float width=max(1.0,float(body.width)*abs(cos(body.angle))+float(body.height)*abs(sin(body.angle)));
    float rho=support.y/max(support.z,1.0);
    float capacity=0.25*rho*min(width,support.z)*(2.0+8.0*friction);
    float pressure=0.25/(max(body.inverse_mass,1e-12)*width);
    float excess=max(pressure/max(capacity,0.01)-1.0,0.0);
    return min(0.05,0.005*excess);
}
