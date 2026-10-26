#ifndef FERRUM_FIRE_GAS_CHECKPOINT_H
#define FERRUM_FIRE_GAS_CHECKPOINT_H
#include <stdint.h>
/** Cold authoritative chunk stock, compensated energy, geometry/work and face
 * valves. Derived pressure/transport caches are deliberately not persisted. */
typedef struct {
    float cells[4096][16];
    uint32_t gates[4096];
} fr_gas_checkpoint_chunk_t;
#endif
