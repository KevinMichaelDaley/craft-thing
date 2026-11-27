struct Particle {
    int x_fp, y_fp, vx_fp, vy_fp;
    float f00, f01, f10, f11;
    uint id_lo, id_hi, mass_fp, grain_fp, material, flags;
};
layout(set=0,binding=19,std430) DC_GRAIN_ACCESS buffer Particles { Particle particles[]; };
layout(buffer_reference,std430,buffer_reference_align=4) DC_GRAIN_ACCESS buffer GrainParticles { Particle records[]; };
Particle read_grain(uint index) {
    if ((index & HYDRO_PEER) != 0u)
        return GrainParticles(hydro_address(DC_HYDRO_PARTICLES)).records[index & ~HYDRO_PEER];
    return particles[index];
}
