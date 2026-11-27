#ifndef FERRUM_FIRE_GAS_CHECKPOINT_H
#define FERRUM_FIRE_GAS_CHECKPOINT_H
#include <stdint.h>
/** Geometry words in the cold gas packet. */
enum { FR_GAS_CHECKPOINT_VOLUME_WORD = 12u, FR_GAS_CHECKPOINT_PREVIOUS_VOLUME_WORD = 13u };
typedef struct {uint64_t physical_steps,substeps;double advanced_seconds;} fr_gas_gpu_clock_t;
/** Cold authoritative chunk stock, compensated energy, geometry/work and face
 * valves. Derived pressure/transport caches are deliberately not persisted. */
typedef struct {
    float cells[4096][16];
    uint32_t gates[4096];
    fr_gas_gpu_clock_t clock;
    /** Low words of the exhaust and soot stocks, in that order. */
    float carrier_tail[4096][2];
} fr_gas_checkpoint_chunk_t;
#endif
