#include "dungeoncraft/chunk.h"

void dc_cell_address(int64_t x, int64_t y, dc_chunk_coord_t *coord,
                     uint32_t *local_x, uint32_t *local_y) {
    (void)x; (void)y; (void)coord; (void)local_x; (void)local_y;
}
bool dc_chunk_table_init(dc_chunk_table_t *table, uint32_t capacity) {
    (void)table; (void)capacity; return false;
}
void dc_chunk_table_destroy(dc_chunk_table_t *table) { (void)table; }
bool dc_chunk_table_find(const dc_chunk_table_t *table, dc_chunk_coord_t coord,
                         uint32_t *index) { (void)table; (void)coord; (void)index; return false; }
bool dc_chunk_table_begin_load(dc_chunk_table_t *table, dc_chunk_coord_t coord,
                               uint32_t *index, uint64_t *generation) {
    (void)table; (void)coord; (void)index; (void)generation; return false;
}
bool dc_chunk_table_finish_load(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation) {
    (void)table; (void)index; (void)generation; return false;
}
bool dc_chunk_table_set_active(dc_chunk_table_t *table, uint32_t index, bool active) {
    (void)table; (void)index; (void)active; return false;
}
bool dc_chunk_table_mark_dirty(dc_chunk_table_t *table, uint32_t index) {
    (void)table; (void)index; return false;
}
bool dc_chunk_table_finish_save(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation) {
    (void)table; (void)index; (void)generation; return false;
}
bool dc_chunk_table_evict(dc_chunk_table_t *table, uint32_t index,
                          uint64_t completed_gpu_value) {
    (void)table; (void)index; (void)completed_gpu_value; return false;
}
