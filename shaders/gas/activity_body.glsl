#include "acoustic.glsl"
shared uint needed;
void main() {
    uint lane=gl_LocalInvocationIndex;if(lane==0u) needed=0u;barrier();
    uint width=(params.width+7u)/8u;
    uint tile=(params.bank&512u)!=0u?active_tiles[gl_WorkGroupID.x]:gl_WorkGroupID.y*width+gl_WorkGroupID.x;
    uvec2 p=uvec2(tile%width,tile/width)*8u+uvec2(lane%8u,lane/8u);
    if(p.x<params.width&&p.y<params.height) {
        Primitive own=gas_cached(ivec2(p));bool practical=(params.bank&FR_GAS_PRACTICAL_BIT)!=0u;
        bool awake=practical?any(greaterThan(abs(own.hydro.yz),vec2(.05))):any(notEqual(own.hydro.yz,vec2(0)));
#ifdef FR_GAS_CACHED_NEIGHBORS
        if(own.metric.w>0.&&!awake) {
#else
        if(own.metric.w>0.) {
#endif
            float fraction=gas_parameter(FR_GAS_OXYGEN_WORD);
            float oxygen=(fraction/gas_parameter(0u))/(fraction/gas_parameter(0u)+(1.-fraction)/gas_parameter(1u));
            float reference=(oxygen*gas_parameter(0u)+(1.-oxygen)*gas_parameter(1u))*gas_parameter(FR_GAS_AMBIENT_WORD);
            float ratio=gas_parameter(FR_GAS_KINETIC_WORD)*own.hydro.w/(own.hydro.x*reference);
            awake=awake||(gas_parameter(FR_GAS_GRAVITY_WORD)!=0.&&abs(ratio-1.)>(practical?.001:0.00000095367431640625));
#ifdef FR_GAS_CACHED_NEIGHBORS
            if(!awake) {
                uint own_atlas=gas_atlas(ivec2(p)),own_gate=own_atlas==0xffffffffu?0u:gates[own_atlas];
                for(uint d=0u;d<4u&&!awake;d++) {
                    ivec2 q=ivec2(p)+gas_direction(d);Primitive other=gas_cached(q);
                    /* Completed source caches encode missing pages with zero volume. */
                    if(other.metric.w<=0.) continue;
                    bool open=(d&1u)==0u?(own_gate&(1u<<(d/2u)))==0u:gas_gate_open(q,d/2u);
                    if(open) awake=practical?abs(other.hydro.w-own.hydro.w)>.001*max(abs(other.hydro.w),abs(own.hydro.w)):other.hydro.w!=own.hydro.w;
                }
            }
#else
            for(uint d=0u;d<4u;d++) {
                uint j=0u;if(gas_neighbor(ivec2(p),d,j)) {
                    float other=gas_cached(ivec2(p)+gas_direction(d)).hydro.w;
                    if(practical?abs(other-own.hydro.w)>.001*max(abs(other),abs(own.hydro.w)):other!=own.hydro.w) awake=true;
                }
            }
#endif
        }
        if(awake) atomicOr(needed,1u);
    }
    barrier();if(lane==0u) {
        activity[tile]=needed;
#ifdef FR_GAS_CACHED_NEIGHBORS
        if(gl_WorkGroupID.x==0u&&gl_WorkGroupID.y==0u) stats[FR_GAS_ROW_NORM_AFTER_WORD]=1u;
#endif
    }
}
