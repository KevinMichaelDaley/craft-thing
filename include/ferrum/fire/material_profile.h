#ifndef FERRUM_FIRE_MATERIAL_PROFILE_H
#define FERRUM_FIRE_MATERIAL_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    FR_REACTIVE_SOLID,
    FR_REACTIVE_GRANULAR,
    FR_REACTIVE_LIQUID,
    FR_REACTIVE_GAS,
    FR_REACTIVE_PHASE_COUNT
};
enum {
    FR_REACTIVE_FUEL = 1u,
    FR_REACTIVE_WATER_DOUSEABLE = 2u,
    FR_REACTIVE_BOUND_OXIDIZER = 4u,
    FR_REACTIVE_RIGID_CAPABLE = 8u
};

/** A 128-byte GPU-transferable profile. The four product yields partition fuel
 * mass; oxidizer incorporated into exhaust is additional product mass. */
typedef struct {
    uint32_t material, legacy_body_material, phase, flags;
    float density, heat_capacity, conductivity, ignition_K, hazard_per_second;
    float fuel_fraction, chemical_energy, primary_heat, oxygen_per_fuel;
    float bound_oxidizer_fraction, fuel_yields[4];
    float electrical_conductivity, moisture_capacity, oxidized_hazard_scale;
    float greek_corrosion_susceptibility, saturated_hazard_factor, oil_hazard_factor;
    float yield_stress, consistency, shear_exponent, softening_K;
    float hot_yield_multiplier, cold_adhesion, hot_adhesion, surface_burn_per_second;
} fr_reactive_profile_t;

/** Instantaneous reference inputs, independent of flame/smoke visual pigment.
 * Shock energy is a prospective heat input, not another persistent fuel source.
 * Once deposited by the energy stage, supply zero here to avoid double charging. */
typedef struct {
    float temperature_K, saturation, coating_coverage;
    float substrate_oxidation, coating_oxidation;
    float shock_energy_per_mass, oxygen_mass_fraction, bound_oxidizer_per_fuel;
} fr_reactive_exposure_t;

/** Separate hazards retain which finite fuel reservoir could ignite. */
typedef struct { float substrate, coating; } fr_reactive_ignition_rates_t;

/** Number of registered profiles; material IDs are terrain registry IDs. */
uint32_t fr_reactive_profile_count(void);
/** Read an immutable profile; return NULL for an unregistered material ID. */
const fr_reactive_profile_t *fr_reactive_profile(uint32_t material);
/** Map the existing body-material namespace explicitly; unknown IDs return NULL.
 * Additional reactive body materials are admitted by the later geometry stage. */
const fr_reactive_profile_t *fr_reactive_profile_for_body(uint32_t body_material);
/** Return a stable material label, or NULL for an unregistered material. */
const char *fr_reactive_material_name(uint32_t material);
/** Validate nonnegative finite coefficients, composition and retained-char energy. */
bool fr_reactive_profile_valid(const fr_reactive_profile_t *profile,
                               char *err_buf, uint32_t err_cap);
/** CPU reference for profile modifiers. Runtime reactions use GPU state/ledgers.
 * On invalid input, return false without changing the output rates. */
bool fr_reactive_ignition_rates(const fr_reactive_profile_t *substrate,
                                const fr_reactive_profile_t *coating,
                                const fr_reactive_exposure_t *exposure,
                                fr_reactive_ignition_rates_t *out,
                                char *err_buf, uint32_t err_cap);

#endif
