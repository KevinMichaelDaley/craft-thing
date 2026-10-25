#ifndef FERRUM_FIRE_GAS_H
#define FERRUM_FIRE_GAS_H
#include <stdbool.h>
#include <stdint.h>

enum {FR_GAS_OXYGEN,FR_GAS_NITROGEN,FR_GAS_EXHAUST,FR_GAS_VAPOR,FR_GAS_SOOT,FR_GAS_SPECIES};
enum {FR_GAS_FLUX_HLLE,FR_GAS_FLUX_HLLC};
/** Thermodynamic coefficients and the conversion from grid kinetic energy to
 * material heat units. Production units/sound policy are selected by the
 * numerical gate, rather than hidden inside the reference integrator. */
typedef struct {
    double gas_constant[FR_GAS_SPECIES],heat_capacity_cv[FR_GAS_SPECIES];
    double kinetic_scale;
} fr_gas_model_t;
/** Extensive finite-volume state. Pigment channels are RGB-weighted exhaust
 * and soot mass; they are neither gas species nor an additional mass stock. */
typedef struct {
    double species[FR_GAS_SPECIES],energy,momentum[2],pigment[3];
} fr_gas_state_t;
typedef struct {
    double mass,temperature_K,pressure,sound_speed,oxygen_mole_fraction;
} fr_gas_thermo_t;
/** Derive mixture thermodynamics, rejecting unphysical input transactionally. */
bool fr_gas_thermodynamics(const fr_gas_model_t *model,const fr_gas_state_t *state,
    double volume,fr_gas_thermo_t *out,char *err_buf,uint32_t err_cap);
/** Reference reversible volume change; returned work is energy admitted by the
 * moving boundary. Species, momentum and pigment remain extensive. */
bool fr_gas_reference_compress(const fr_gas_model_t *model,const fr_gas_state_t *state,
    double old_volume,double new_volume,fr_gas_state_t *out,double *work,
    char *err_buf,uint32_t err_cap);
/** Reference-atmosphere buoyancy impulse. Ambient gas has zero acceleration;
 * composition and temperature determine density at reference pressure. Work is
 * the exact kinetic energy admitted by the external gravitational field. */
bool fr_gas_reference_buoyancy(const fr_gas_model_t *model,const fr_gas_state_t *state,
    double volume,double ambient_specific_RT,double gravity,double seconds,
    fr_gas_state_t *out,double *work,char *err_buf,uint32_t err_cap);
/** Debit an admitted fraction of a finite packet and mix its conserved fields.
 * Invalid requests change neither output; no inventory is silently discarded. */
bool fr_gas_admit(const fr_gas_model_t *model,const fr_gas_state_t *source,
    const fr_gas_state_t *target,double fraction,fr_gas_state_t *source_out,
    fr_gas_state_t *target_out,char *err_buf,uint32_t err_cap);
/** Cold/reference multidimensional CFL bound; never a CPU runtime field scan. */
bool fr_gas_reference_max_dt(const fr_gas_model_t *model,const fr_gas_state_t *states,
    const double *volumes,uint32_t width,uint32_t height,double cell_size,
    double courant,double *out,char *err_buf,uint32_t err_cap);
/** Small closed-wall finite-volume reference. Production transport must remain
 * on the GPU; this CPU oracle is only for numerical/conservation validation. */
bool fr_gas_reference_step(const fr_gas_model_t *model,const fr_gas_state_t *source,
    const double *volumes,uint32_t width,uint32_t height,double cell_size,
    double seconds,uint32_t solver,fr_gas_state_t *out,char *err_buf,uint32_t err_cap);
/** Cold conservative acoustic-pressure oracle. Acoustic stiffness is implicit;
 * explicit species/momentum transport still obeys the advective CFL bound. */
bool fr_gas_implicit_reference_step(const fr_gas_model_t *model,const fr_gas_state_t *source,
    const double *void_fractions,uint32_t width,uint32_t height,double seconds,
    fr_gas_state_t *out,char *err_buf,uint32_t err_cap);
/** Cold large-CFL conservative directional transport oracle. This is a
 * numerical gate only; production field transport must stay on the GPU. */
bool fr_gas_transport_reference_step(const fr_gas_model_t *model,const fr_gas_state_t *source,
    const double *void_fractions,uint32_t width,uint32_t height,double seconds,
    fr_gas_state_t *out,char *err_buf,uint32_t err_cap);
/** Cold composed large-CFL transport/acoustic numerical gate. */
bool fr_gas_large_cfl_reference_step(const fr_gas_model_t *model,const fr_gas_state_t *source,
    const double *void_fractions,uint32_t width,uint32_t height,double seconds,
    fr_gas_state_t *out,char *err_buf,uint32_t err_cap);
#endif
