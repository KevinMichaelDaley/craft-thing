#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/thermal_checkpoint.h"
bool fr_thermal_checkpoint_encode(const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,
    uint32_t count,uint8_t **bytes,size_t *length,char *err,uint32_t cap) {
    (void)state;(void)chunks;(void)count;(void)bytes;(void)length;(void)err;(void)cap;return false;
}
bool fr_thermal_checkpoint_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t count,fr_thermal_checkpoint_t *out,char *err,uint32_t cap) {
    (void)bytes;(void)length;(void)chunks;(void)count;(void)out;(void)err;(void)cap;return false;
}
void fr_thermal_checkpoint_release(fr_thermal_checkpoint_t *state) {
    if(state) {free(state->water_excess);memset(state,0,sizeof(*state));}
}
