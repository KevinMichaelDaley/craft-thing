#include "acoustic.glsl"
#ifndef FR_GAS_TEMPORAL_TILE_SIZE
#define FR_GAS_TEMPORAL_TILE_SIZE 8u
#define FR_GAS_TEMPORAL_HALO 4u
#define FR_GAS_TEMPORAL_LIST_BASE 0u
#endif
#define FR_GAS_TEMPORAL_EXTENT (FR_GAS_TEMPORAL_TILE_SIZE+2u*FR_GAS_TEMPORAL_HALO)
#ifdef FR_GAS_HALF_PRESSURE
layout(set=0,binding=24,std430) restrict buffer TemporalWork {uvec4 half_work[];};
float temporal_value(uint at) {return uintBitsToFloat(half_work[at/4u][at%4u]);}
void temporal_write(uint at,float value) {half_work[at/4u][at%4u]=floatBitsToUint(value);}
#else
layout(set=0,binding=24,std430) restrict buffer TemporalWork {float temporal[];};
float temporal_value(uint at) {return temporal[at];}
void temporal_write(uint at,float value) {temporal[at]=value;}
#endif
shared float values[FR_GAS_TEMPORAL_EXTENT*FR_GAS_TEMPORAL_EXTENT];
float source_value(uint at) {return (params.bank&FR_GAS_TEMPORAL_SOURCE_BIT)!=0u?temporal_value(at):correction[at];}
void main() {
    uint tile=active_tiles[FR_GAS_TEMPORAL_LIST_BASE+gl_WorkGroupID.x],width=(params.width+FR_GAS_TEMPORAL_TILE_SIZE-1u)/FR_GAS_TEMPORAL_TILE_SIZE,lane=gl_LocalInvocationIndex;
    ivec2 origin=ivec2(int(tile%width),int(tile/width))*int(FR_GAS_TEMPORAL_TILE_SIZE)-int(FR_GAS_TEMPORAL_HALO);
    uint row=lane/(FR_GAS_TEMPORAL_EXTENT/2u),red=row*FR_GAS_TEMPORAL_EXTENT+2u*(lane%(FR_GAS_TEMPORAL_EXTENT/2u))+(row&1u),black=row*FR_GAS_TEMPORAL_EXTENT+2u*(lane%(FR_GAS_TEMPORAL_EXTENT/2u))+((row+1u)&1u);
    uint slots[2]=uint[2](red,black);uint indices[2];bool valid[2],awake[2];vec4 coefficients[2],data[2];
    for(uint color=0u;color<2u;color++) {
        uint slot=slots[color];ivec2 p=origin+ivec2(int(slot%FR_GAS_TEMPORAL_EXTENT),int(slot/FR_GAS_TEMPORAL_EXTENT));
        valid[color]=all(greaterThanEqual(p,ivec2(0)))&&all(lessThan(p,ivec2(params.width,params.height)));
        uint at=valid[color]?uint(p.y)*params.width+uint(p.x):0u;indices[color]=at;
        values[slot]=valid[color]?source_value(at):0.;awake[color]=false;coefficients[color]=vec4(0);data[color]=vec4(0);
        uint first_margin=color+1u,x=slot%FR_GAS_TEMPORAL_EXTENT,y=slot/FR_GAS_TEMPORAL_EXTENT;
        if(valid[color]&&x>=first_margin&&x<FR_GAS_TEMPORAL_EXTENT-first_margin&&y>=first_margin&&y<FR_GAS_TEMPORAL_EXTENT-first_margin) {
            uint own_tile=(uint(p.y)/8u)*((params.width+7u)/8u)+uint(p.x)/8u;
            awake[color]=(activity[gas_tile_count()+own_tile]&FR_GAS_SOLVE_TILE_BIT)!=0u;
            if(awake[color]) {
#ifdef FR_GAS_HALF_PRESSURE
                uvec4 encoded=half_work[4u*params.width*params.height+at];float inverse=uintBitsToFloat(encoded.w);
                if(inverse<0.) {coefficients[color]=stencil[at];data[color]=correction_data[at];}
                else {coefficients[color]=vec4(unpackHalf2x16(encoded.x),unpackHalf2x16(encoded.y));data[color]=vec4(uintBitsToFloat(encoded.z),inverse,1.8,0);}
#elif defined(FR_GAS_PACKED_PRESSURE)
                uint base=14u*params.width*params.height+6u*at;
                coefficients[color]=vec4(temporal[base],temporal[base+1u],temporal[base+2u],temporal[base+3u]);
                data[color]=vec4(temporal[base+4u],temporal[base+5u],1.8,0);
#else
                coefficients[color]=stencil[at];data[color]=correction_data[at];
#endif
            }
        }
    }
    barrier();
    for(uint phase=0u;phase<FR_GAS_TEMPORAL_HALO;phase++) {
        uint color=phase&1u,slot=slots[color],x=slot%FR_GAS_TEMPORAL_EXTENT,y=slot/FR_GAS_TEMPORAL_EXTENT,margin=phase+1u;
        vec4 weights=coefficients[color],cell=data[color];
        if(awake[color]&&cell.y>0.&&x>=margin&&x<FR_GAS_TEMPORAL_EXTENT-margin&&y>=margin&&y<FR_GAS_TEMPORAL_EXTENT-margin) {
            float rhs=cell.x;
            for(uint d=0u;d<4u;d++) {
                if(weights[d]==0.) continue;
                uint neighbor=d==0u?slot+1u:(d==1u?slot-1u:(d==2u?slot+FR_GAS_TEMPORAL_EXTENT:slot-FR_GAS_TEMPORAL_EXTENT));
                rhs+=weights[d]*values[neighbor];
            }
            float relaxation=(params.bank&FR_GAS_MG_BOUNDARY_BIT)!=0u?1.5:cell.z;
            values[slot]+=relaxation*(rhs*cell.y-values[slot]);
        }
        barrier();
    }
    for(uint color=0u;color<2u;color++) {
        uint slot=slots[color],x=slot%FR_GAS_TEMPORAL_EXTENT,y=slot/FR_GAS_TEMPORAL_EXTENT;
        if(valid[color]&&x>=FR_GAS_TEMPORAL_HALO&&x<FR_GAS_TEMPORAL_EXTENT-FR_GAS_TEMPORAL_HALO&&y>=FR_GAS_TEMPORAL_HALO&&y<FR_GAS_TEMPORAL_EXTENT-FR_GAS_TEMPORAL_HALO) {
            if((params.bank&FR_GAS_TEMPORAL_SOURCE_BIT)!=0u) correction[indices[color]]=values[slot];
            else temporal_write(indices[color],values[slot]);
        }
    }
}
