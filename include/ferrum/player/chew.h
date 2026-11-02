#ifndef FERRUM_PLAYER_CHEW_H
#define FERRUM_PLAYER_CHEW_H

#include <stdbool.h>
#include <stdint.h>

/** Freshly validated, localized tree parcel. Mass comes from its authoritative
 * dry inventory, not a count of canvas pixels or an assumed RPG attribute.
 * eligible includes reach, tree membership and current world availability. */
typedef struct {
    uint64_t parcel_id;
    uint32_t generation,material,dry_mass_fp;
    bool eligible;
} fr_chew_target_t;
/** Caller-owned single-bite work. Units are Q16 dry mass times milliseconds.
 * No world mutation, parcel transfer or inventory reward occurs in this layer. */
typedef struct {
    fr_chew_target_t target;
    uint64_t work;
} fr_chew_work_t;

/** Advance only the time spent in a valid chewing work phase. Explicit effort
 * is Q16 dry mass per second. Release pauses partial work on the same parcel;
 * missing/ineligible/changed targets discard it. Work saturates at one bite,
 * so a stall cannot bank removal of other parcels. Ready requires fresh held
 * intent and eligibility. Invalid arguments leave both outputs unchanged. */
bool fr_chew_work_advance(fr_chew_work_t *work,const fr_chew_target_t *target,
    uint32_t effort_fp_per_second,uint64_t elapsed_ms,bool held,bool *ready);
/** Acknowledge a successful world transaction for the exact ready parcel.
 * Revalidate reach/ownership and reserve inventory/structural release in that
 * transaction first. A failed world transaction must not call this function.
 * Repeated/stale acknowledgements return false without changing work. */
bool fr_chew_work_committed(fr_chew_work_t *work,const fr_chew_target_t *target);

#endif
