#ifndef FERRUM_FIRE_THERMAL_H
#define FERRUM_FIRE_THERMAL_H
#include "dungeoncraft/thermal_parameters.h"
#include "ferrum/fire/gas.h"
#include "ferrum/fire/reactive_state.h"

/** Extensive energy and actual heat capacity of one physical thermal sample. */
typedef struct {float energy,capacity,conductivity;} fr_thermal_sample_t;
/** Finite external heat reservoir; excess energy is measured above ambient. */
typedef struct {float excess_energy,capacity;} fr_thermal_probe_t;

/** Derive a thermal sample without using visual pigment or dry reference mass
 * as additional inventory. Failure leaves out unchanged. */
bool fr_thermal_sample_from_state(const fr_reactive_state_t *state,
    fr_thermal_sample_t *out,char *err_buf,uint32_t err_cap);
/** Conservative, signed first-to-second heat flux across one cell face. A
 * four-neighbor monotonicity bound prevents large time steps overshooting. */
bool fr_thermal_pair_flux(fr_thermal_sample_t first,fr_thermal_sample_t second,
    float seconds,float *energy_to_second,char *err_buf,uint32_t err_cap);
/** Reference for finite source contact; no source or target temperature clamp.
 * Both outputs publish together and source+target energy is conserved. */
bool fr_thermal_probe_exchange(fr_thermal_probe_t probe,fr_thermal_sample_t target,
    float seconds,fr_thermal_probe_t *probe_out,fr_thermal_sample_t *target_out,
    char *err_buf,uint32_t err_cap);
/** Conservative wetting of a solid by pure liquid water across one face.
 * Water mass and its proportional enthalpy enter the solid together. Outputs
 * retain parcel/origin/hazard history and publish only on complete success. */
bool fr_thermal_absorb_pair(const fr_reactive_state_t *solid,const fr_reactive_state_t *water,
    float seconds,fr_reactive_state_t *solid_out,fr_reactive_state_t *water_out,
    char *err_buf,uint32_t err_cap);
/** Transfer a requested finite amount of bound water into gas vapor. The gas
 * keeps its sensible energy; vapor carries a separate internal-energy offset
 * returned by fr_thermal_vapor_reference_energy(). Outputs publish together. */
bool fr_thermal_evaporate_pair(const fr_reactive_state_t *solid,const fr_gas_state_t *gas,
    const fr_gas_model_t *model,uint32_t requested_water_fp,fr_reactive_state_t *solid_out,
    fr_gas_state_t *gas_out,uint32_t *transferred_water_fp,char *err_buf,uint32_t err_cap);
/** Vapor reference offset in kJ/kg for a gas state whose energy stores cv*T. */
bool fr_thermal_vapor_reference_energy(const fr_gas_model_t *model,double *out,
    char *err_buf,uint32_t err_cap);
#endif
