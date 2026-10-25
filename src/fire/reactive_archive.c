#include <stdlib.h>
#include "ferrum/fire/reactive_archive.h"
bool fr_reactive_archive_encode(const fr_reactive_archive_t *archive,const dc_chunk_t *chunks,
    uint32_t chunk_count,uint8_t **bytes,size_t *length,char *err,uint32_t cap) {
    (void)archive;(void)chunks;(void)chunk_count;(void)bytes;(void)length;(void)err;(void)cap;return false;
}
bool fr_reactive_archive_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t chunk_count,fr_reactive_archive_t *out,char *err,uint32_t cap) {
    (void)bytes;(void)length;(void)chunks;(void)chunk_count;(void)out;(void)err;(void)cap;return false;
}
void fr_reactive_archive_release(fr_reactive_archive_t *archive) {
    if(!archive) return;
    for(uint32_t i=0;i<archive->chunk_count;i++) fr_reactive_chunk_release(archive->chunks+i);
    free(archive->chunks);free(archive->bodies);*archive=(fr_reactive_archive_t){0};
}
