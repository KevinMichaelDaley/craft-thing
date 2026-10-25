#include <stdio.h>
#include "smoke_box.h"
fr_gas_model_t dc_smoke_box_model(void) {return (fr_gas_model_t){0};}
bool dc_smoke_box_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_smoke_box_chunk_t *out,char *err,uint32_t cap) {
    (void)coords;(void)count;(void)out;if(err&&cap) snprintf(err,cap,"Smoke box fixture not implemented");return false;
}
void dc_smoke_box_release(dc_smoke_box_chunk_t *chunks,uint32_t count) {(void)chunks;(void)count;}
