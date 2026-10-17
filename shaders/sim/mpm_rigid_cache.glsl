bool mpm_rigid_cache_clean() {
    uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
    uint base=8u+2u*w*h+4096u*8u+12u+8192u*32u+16u+64u*32u+8192u*16u;
    return params.unused0!=0u&&data[base+STRUCTURAL_COMPONENT_DIRTY_WORD]==0u;
}
