#include "pressure_coarse.glsl"
shared float values[FR_GAS_COARSE_GROUP_CAPACITY];
void main() {
    uint rank=gl_LocalInvocationIndex,count=gas_tile_count(),width=coarse_width(),height=(params.height+7u)/8u;
    if(count>FR_GAS_COARSE_GROUP_CAPACITY) return;
    for(uint at=rank;at<count;at+=FR_GAS_LOCAL_SIZE) values[at]=coarse[at].data.z;
    barrier();uint half_width=(width+1u)/2u;
    for(uint sweep=0u;sweep<32u;sweep++) for(uint color=0u;color<2u;color++) {
        for(uint cell=rank;cell<half_width*height;cell+=FR_GAS_LOCAL_SIZE) {
            uint y=cell/half_width,x=2u*(cell%half_width)+((y+color)&1u);if(x>=width) continue;
            uint at=y*width+x;CoarseRow row=coarse[at];if(row.data.y<=0.) continue;
            float rhs=row.data.x;
            for(uint d=0u;d<4u;d++) if(row.coefficients[d]>0.) {
                uint j=d==0u?at+1u:(d==1u?at-1u:(d==2u?at+width:at-width));
                rhs+=row.coefficients[d]*values[j];
            }
            values[at]+=1.8*(rhs/row.data.y-values[at]);
        }
        barrier();
    }
    for(uint at=rank;at<count;at+=FR_GAS_LOCAL_SIZE) coarse[at].data.z=values[at];
}
