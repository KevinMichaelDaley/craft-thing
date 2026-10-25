#ifndef FERRUM_FIRE_REACTIVE_ARCHIVE_H
#define FERRUM_FIRE_REACTIVE_ARCHIVE_H
#include "ferrum/fire/reactive_chunk.h"

/** Immutable checkpoint channels; chunk array order matches physical chunks.
 * Bodies use logical identity/generation/rest coordinates, never GPU addresses. */
typedef struct {
    uint64_t next_id;
    uint32_t body_count,chunk_count;
    fr_reactive_record_t *bodies;
    fr_reactive_chunk_t *chunks;
} fr_reactive_archive_t;
/** Encode a globally validated checkpoint into an owned byte allocation.
 * Failure leaves both output arguments unchanged. */
bool fr_reactive_archive_encode(const fr_reactive_archive_t *archive,const dc_chunk_t *chunks,
    uint32_t chunk_count,uint8_t **bytes,size_t *length,char *err_buf,uint32_t err_cap);
/** Decode all versions, inventories and bindings before publishing owned output. */
bool fr_reactive_archive_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t chunk_count,fr_reactive_archive_t *out,char *err_buf,uint32_t err_cap);
/** Release all owned archive channels and clear the archive. */
void fr_reactive_archive_release(fr_reactive_archive_t *archive);
#endif
