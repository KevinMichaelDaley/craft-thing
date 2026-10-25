#include <stdlib.h>
#include "ferrum/fire/reactive_chunk.h"

bool fr_reactive_chunk_valid(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,char *err,uint32_t cap) {
    (void)channel;(void)chunk;(void)err;(void)cap;return false;
}
bool fr_reactive_chunk_encode(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,
    uint8_t *bytes,size_t capacity,size_t *written,char *err,uint32_t cap) {
    (void)channel;(void)chunk;(void)bytes;(void)capacity;(void)written;(void)err;(void)cap;return false;
}
bool fr_reactive_chunk_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunk,
    fr_reactive_chunk_t *out,char *err,uint32_t cap) {
    (void)bytes;(void)length;(void)chunk;(void)out;(void)err;(void)cap;return false;
}
bool fr_reactive_chunk_size(uint32_t count,size_t *out) {(void)count;(void)out;return false;}
void fr_reactive_chunk_release(fr_reactive_chunk_t *channel) {
    if(channel) {free(channel->attachments);*channel=(fr_reactive_chunk_t){0};}
}
