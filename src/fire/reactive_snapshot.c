#include "ferrum/fire/reactive_state.h"

bool fr_reactive_snapshot_write(const char *path,const fr_reactive_record_t *records,
                                 uint32_t count,uint64_t next,char *err,uint32_t cap) {
    (void)path;(void)records;(void)count;(void)next;(void)err;(void)cap;return false;
}
bool fr_reactive_snapshot_read(const char *path,fr_reactive_record_t *records,uint32_t capacity,
                                uint32_t *count,uint64_t *next,uint32_t *required,char *err,uint32_t cap) {
    (void)path;(void)records;(void)capacity;(void)count;(void)next;(void)required;(void)err;(void)cap;return false;
}
