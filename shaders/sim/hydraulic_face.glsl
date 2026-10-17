/* Soil has 1/64 of open-water face conductivity. This throttles flux without
   changing pore capacity or deleting water. Both sides of a face contribute. */
const uint SOIL_CONDUCTIVITY_DIVISOR=64u;
uint hydraulic_material(ivec2 p) {
    uint page=page_at(p);
    return page==0u?0u:chunk_cells[atlas_index(p,page)].x&65535u;
}
uint hydraulic_aperture(uint aperture,ivec2 face,ivec2 direction) {
    return hydraulic_material(face)==4u || hydraulic_material(face+abs(direction))==4u?
        aperture/SOIL_CONDUCTIVITY_DIVISOR:aperture;
}
