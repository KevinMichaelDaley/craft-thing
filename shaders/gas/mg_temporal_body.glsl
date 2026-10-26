#include "multigrid.glsl"
shared float values[144];
void main() {
    uint level=mg_level(),lane=gl_LocalInvocationIndex;uvec2 size=mg_size(level);
    ivec2 origin=ivec2(gl_WorkGroupID.xy)*8-2;
    bool temporary=(params.bank&(1u<<24u))!=0u;
    uint slots[3];bool valid[3];vec4 weights[3];vec2 data[3];
    for(uint k=0u;k<3u;k++) {
        uint slot=lane+k*64u;slots[k]=slot;valid[k]=false;weights[k]=vec4(0);data[k]=vec2(0);
        if(slot>=144u) continue;
        ivec2 p=origin+ivec2(int(slot%12u),int(slot/12u));
        bool inside=all(greaterThanEqual(p,ivec2(0)))&&all(lessThan(p,ivec2(size)));
        uint at=inside?uint(p.y)*size.x+uint(p.x):0u;
        values[slot]=inside?mg_value(level,at,temporary):0.;
        if(inside&&mg_valid(level,at)) {valid[k]=true;weights[k]=mg_coeff(level,at);data[k]=vec2(mg_rhs(level,at),mg_diag(level,at));}
    }
    barrier();
    for(uint phase=0u;phase<2u;phase++) {
        float next[3];uint margin=phase+1u;
        for(uint k=0u;k<3u;k++) {
            uint slot=slots[k];next[k]=0.;if(slot>=144u) continue;
            uint x=slot%12u,y=slot/12u;next[k]=values[slot];
            if(valid[k]&&x>=margin&&x<12u-margin&&y>=margin&&y<12u-margin) {
                float rhs=data[k].x;vec4 coefficients=weights[k];
                for(uint d=0u;d<4u;d++) if(coefficients[d]>0.) {
                    uint neighbor=d==0u?slot+1u:(d==1u?slot-1u:(d==2u?slot+12u:slot-12u));
                    rhs+=coefficients[d]*values[neighbor];
                }
                next[k]=mix(values[slot],rhs/data[k].y,.75);
            }
        }
        barrier();for(uint k=0u;k<3u;k++) if(slots[k]<144u) values[slots[k]]=next[k];barrier();
    }
    uvec2 p=gl_WorkGroupID.xy*8u+uvec2(lane%8u,lane/8u);
    if(all(lessThan(p,size))) mg_write(level,p.y*size.x+p.x,!temporary,values[(lane/8u+2u)*12u+lane%8u+2u]);
}
