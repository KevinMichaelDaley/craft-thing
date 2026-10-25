#ifndef DC_MPM_PARTICLE_MIRROR_GLSL
#define DC_MPM_PARTICLE_MIRROR_GLSL
layout(set=0,binding=22,std430) buffer ParticleMirror {uint particle_mirror[];};
const uint MPM_PARTICLE_WORDS=14u,MPM_PARTICLE_MASS_WORD=10u;
/* The alternate pool can retain the preceding half-step. External deletions
   must remove its occupancy too before any sparse pass changes bindings. */
void mpm_forget_output(uint particle) {
    reactive_forget_particle(particle);
    particle_mirror[particle*MPM_PARTICLE_WORDS+MPM_PARTICLE_MASS_WORD]=0u;
}
#endif
