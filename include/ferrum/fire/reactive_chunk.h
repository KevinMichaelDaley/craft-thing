#ifndef FERRUM_FIRE_REACTIVE_CHUNK_H
#define FERRUM_FIRE_REACTIVE_CHUNK_H

#include "ferrum/fire/reactive_state.h"

/** Serialized physical row, not an arena handle or GPU address. Particle birth
 * identities distinguish repeated brush PIDs; rows bind each saved instance. */
typedef struct {
    uint32_t local_index;
    fr_reactive_record_t record;
} fr_reactive_attachment_t;
typedef struct {
    uint32_t count;
    uint64_t next_id;
    fr_reactive_attachment_t *attachments;
} fr_reactive_chunk_t;

/** Validate an immutable chunk channel against its accompanying physical data. */
bool fr_reactive_chunk_valid(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,
    char *err_buf,uint32_t err_cap);
/** Encode one immutable chunk channel, including logical state and row bindings.
 * Failure leaves the caller's destination and written length unchanged. */
bool fr_reactive_chunk_encode(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,
    uint8_t *bytes,size_t capacity,size_t *written,char *err_buf,uint32_t err_cap);
/** Decode and validate before publishing an owned output channel. */
bool fr_reactive_chunk_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunk,
    fr_reactive_chunk_t *out,char *err_buf,uint32_t err_cap);
/** Return the exact encoded size with checked arithmetic. */
bool fr_reactive_chunk_size(uint32_t count,size_t *out);
/** Release an owned channel and clear it. */
void fr_reactive_chunk_release(fr_reactive_chunk_t *channel);

#endif
