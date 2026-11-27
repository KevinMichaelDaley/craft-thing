#include "../../include/dungeoncraft/hydro_peer_layout.h"
const uint HYDRO_PEER = 0x80000000u;
layout(set=0,binding=36,std430) readonly buffer HydroControl {uint hydro_control[];};
layout(buffer_reference,std430,buffer_reference_align=4) buffer HydroWords {uint words[];};
layout(buffer_reference,std430,buffer_reference_align=8) readonly buffer HydroPairs {uvec2 pairs[];};
uvec2 hydro_address(uint field) {
    uint i=DC_HYDRO_ADDRESS_WORD(field);
    return uvec2(hydro_control[i],hydro_control[i+1u]);
}
ivec2 hydro_offset() {return ivec2(hydro_control[DC_HYDRO_PEER_OFFSET],hydro_control[DC_HYDRO_PEER_OFFSET+1u]);}
ivec2 hydro_peer_size() {return ivec2(hydro_control[DC_HYDRO_PEER_WIDTH],hydro_control[DC_HYDRO_PEER_HEIGHT]);}
uint hydro_peer_page(ivec2 p) {
    if(hydro_control[DC_HYDRO_PEER_WIDTH]==0u) return 0u;
    p+=hydro_offset();
    ivec2 size=hydro_peer_size();
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size))) return 0u;
    uint page=HydroWords(hydro_address(DC_HYDRO_PAGES)).words[(p.y/64)*((size.x+63)/64)+p.x/64];
    return page==0u?0u:page|HYDRO_PEER;
}
