bool mpm_rigid_cache_clean() {
    uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
    uint base=8u+2u*w*h+DC_LAYOUT_PAIR_CAPACITY*8u+12u+DC_LAYOUT_CONTACT_CAPACITY*32u+16u+64u*32u+DC_LAYOUT_CONTACT_CAPACITY*16u;
    return params.unused0!=0u&&DC_DATA(base+STRUCTURAL_COMPONENT_DIRTY_WORD)==0u;
}
