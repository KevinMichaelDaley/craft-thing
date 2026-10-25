#ifndef FERRUM_FIRE_REACTIVE_STATE_H
#define FERRUM_FIRE_REACTIVE_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

#define FR_REACTIVE_STATE_ABI 3u
#define FR_REACTIVE_STATE_WORDS 28u
#define FR_REACTIVE_OWNER_WORDS 6u
enum {
    FR_REACTIVE_WATER_MIRROR = 1u,
    FR_REACTIVE_FLAMING = 2u,
    FR_REACTIVE_COATING_FLAMING = 4u,
    FR_REACTIVE_SMOLDERING = 8u,
    FR_REACTIVE_SPENT = 16u
};

/** Persistent parcel data. Species masses are Q16.16 and extensive. dry_mass_fp
 * is reference metadata, not another mass inventory. Color stays independent.
 * ABI 3 retains low-order heat in two formerly reserved words; budget readers
 * use fr_reactive_state_energy() rather than discarding those remainders.
 * A WATER_MIRROR particle caches its existing moisture flag amount: ledgers count
 * that physical bound water once, not once in each representation. */
typedef struct {
    uint32_t material, flags, parcel_id_low, parcel_id_high;
    uint32_t origin_id_low, origin_id_high, identity_reserved[2];
    uint32_t dry_mass_fp, fuel_mass_fp, char_mass_fp, inert_mass_fp;
    uint32_t bound_water_fp, coating_oil_fuel_fp, coating_jelly_fuel_fp;
    uint32_t core_oxidizer_fp, coating_oxidizer_fp;
    float thermal_energy, oxidized_fraction, corroded_fraction;
    float ignition_threshold, ignition_integral;
    float coating_threshold, coating_integral, pending_shock_energy;
    float thermal_energy_residual; /**< Signed low-order extensive heat. */
    float thermal_energy_correction; /**< Retained residual accumulation error. */
    uint32_t reserved[1];
} fr_reactive_state_t;

enum { FR_REACTIVE_OWNER_CELL=1u, FR_REACTIVE_OWNER_BODY=2u,
       FR_REACTIVE_OWNER_PARTICLE=3u };
/** CELL key is chunk x low/high, chunk y low/high, local cell. BODY key is
 * body ID, generation, rest x, rest y, zero. PARTICLE key is stable ID low/high,
 * birth identity low/high, zero. ABI 1 birth generations migrate with high=0.
 * GPU addresses and arena offsets are never saved. */
typedef struct { uint32_t kind, key[5]; } fr_reactive_owner_t;
typedef struct {
    fr_reactive_owner_t owner;
    fr_reactive_state_t state;
} fr_reactive_record_t;

/** Initialize finite composition and thermal energy with stable parcel/origin ID.
 * Caller supplies actual authoritative mass; density never creates matter here. */
bool fr_reactive_state_default(uint32_t material, uint32_t dry_mass_fp,
                                uint32_t bound_water_fp, float temperature_K,
                                uint64_t parcel_id, fr_reactive_state_t *out,
                                char *err_buf, uint32_t err_cap);
/** Sum actual species heat capacities; dry reference mass is excluded. Missing
 * state or unknown material returns zero. This reference has no runtime readback. */
double fr_reactive_state_heat_capacity(const fr_reactive_state_t *state);
/** Read extensive energy including its persisted low-order remainder. */
double fr_reactive_state_energy(const fr_reactive_state_t *state);
/** Transactional cold/reference heat addition with compensated storage.
 * Runtime applies the same arithmetic in its parallel GPU module. */
bool fr_reactive_state_add_heat(fr_reactive_state_t *state,double energy,
    char *err_buf,uint32_t err_cap);
/** Preserve actual particle mass and old soil/sand bound water. Leaf pigment
 * flags are not moisture and are never reinterpreted or modified. */
bool fr_reactive_state_from_particle(const dc_mpm_particle_t *particle,
                                      float temperature_K, uint64_t parcel_id,
                                      fr_reactive_state_t *out,
                                      char *err_buf, uint32_t err_cap);
/** Reject malformed identity, flags, quantities and ignition progress. */
bool fr_reactive_state_valid(const fr_reactive_state_t *state,
                              char *err_buf, uint32_t err_cap);
/** Sum only physical species mass, excluding dry reference metadata. */
uint64_t fr_reactive_state_mass_fp(const fr_reactive_state_t *state);
/** Derive temperature from extensive energy and composition heat capacities. */
bool fr_reactive_state_temperature(const fr_reactive_state_t *state, float *out_K,
                                    char *err_buf, uint32_t err_cap);
/** Reference split: integer species and total energy are retained, lineage and
 * integrated hazards stay unchanged, and caller supplies distinct child IDs.
 * Invalid input leaves both outputs untouched; no runtime CPU solve is implied. */
bool fr_reactive_state_split(const fr_reactive_state_t *source, uint32_t weight_fp,
                              uint64_t left_id, uint64_t right_id,
                              fr_reactive_state_t *left, fr_reactive_state_t *right,
                              char *err_buf, uint32_t err_cap);
/** Construct an exact world-cell owner key, including negative chunk coordinates. */
fr_reactive_owner_t fr_reactive_cell_owner(dc_chunk_coord_t chunk, uint32_t local_cell);
/** Validate a persistent owner key. */
bool fr_reactive_owner_valid(const fr_reactive_owner_t *owner,
                              char *err_buf, uint32_t err_cap);
/** Atomically replace a cold snapshot after validating/sorting unique owner and
 * parcel identities. This serialization is never used as per-frame physics. */
bool fr_reactive_snapshot_write(const char *path, const fr_reactive_record_t *records,
                                 uint32_t count, uint64_t next_parcel_id,
                                 char *err_buf, uint32_t err_cap);
/** Transactional cold read. Missing files initialize old-save ambient defaults
 * (zero records, next ID 1). Insufficient capacity returns required_count without
 * changing records/count/next ID; corrupt/newer input never partially publishes. */
bool fr_reactive_snapshot_read(const char *path, fr_reactive_record_t *records,
                                uint32_t capacity, uint32_t *count,
                                uint64_t *next_parcel_id, uint32_t *required_count,
                                char *err_buf, uint32_t err_cap);
/** Size of the canonical FRS1 packet, including its versioned header. */
bool fr_reactive_snapshot_size(uint32_t count,size_t *out_bytes);
/** Encode an embeddable packet without modifying input records. Invalid data,
 * allocation failure or insufficient capacity leaves bytes/written untouched. */
bool fr_reactive_snapshot_encode(const fr_reactive_record_t *records,uint32_t count,
    uint64_t next_parcel_id,uint8_t *bytes,size_t capacity,size_t *written,
    char *err_buf,uint32_t err_cap);
/** Decode an embedded packet transactionally. Capacity failure reports the
 * required record count; corrupt/newer packets never publish partial state. */
bool fr_reactive_snapshot_decode(const uint8_t *bytes,size_t length,
    fr_reactive_record_t *records,uint32_t capacity,uint32_t *count,
    uint64_t *next_parcel_id,uint32_t *required_count,char *err_buf,uint32_t err_cap);

#endif
