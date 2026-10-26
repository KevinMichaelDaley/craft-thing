#ifndef FERRUM_FIRE_GAS_CHECKPOINT_H
#define FERRUM_FIRE_GAS_CHECKPOINT_H
#include <stdint.h>
typedef struct {uint64_t physical_steps,substeps;double advanced_seconds;} fr_gas_gpu_clock_t;
/** Cold authoritative chunk stock, compensated energy, geometry/work and face
 * valves. Derived pressure/transport caches are deliberately not persisted. */
typedef struct {
    float cells[4096][16];
    uint32_t gates[4096];
    fr_gas_gpu_clock_t clock;
} fr_gas_checkpoint_chunk_t;
#endif
